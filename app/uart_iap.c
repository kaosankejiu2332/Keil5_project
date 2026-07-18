#include "uart_iap.h"
#include "systick.h"
#include "my_uart.h"
#include "my_gd25q32.h"
#include <string.h>

#define IAP_RAM_QUEUE_DEPTH   20U
#define IAP_PACKET_DATA_LEN   128U
#define IAP_PAGE_SIZE         256U

//页缓存，凑两个包写进flash
static uint8_t huancun[260] = {0};
static uint8_t write_info[8] = {0};
//接收到128有效数据包就存储起来
static uint8_t payload_queue[IAP_RAM_QUEUE_DEPTH][IAP_PACKET_DATA_LEN] = {0};
//下一个要取出的位置
static uint8_t payload_queue_head = 0;
//下一个要写入的位置
static uint8_t payload_queue_tail = 0;
//还没被处理的个数
static uint8_t payload_queue_count = 0;
//huancun数组里有多少个字节
static uint16_t page_fill = 0;
static uint8_t eot_pending = 0;
static uint32_t frame_size = 0;
static uint32_t next = 0;
static uint32_t handshake_start = 0;
static uint32_t last_packet_tick = 0;
static uint32_t write_addr = GD25Q32_UPDATE_ADDR;
static uint32_t erase_ahead_addr = GD25Q32_UPDATE_ADDR;
static uint8_t expected_blk = 1;
//拼装完整的Xmodem数据包
static uint8_t xmodem_packet[133] = {0};
//当前拼了多少字节
static uint16_t xmodem_packet_len = 0;
//拼凑缓冲区没数据/满了 --->>>0->没拼凑包:没在组包
static uint8_t xmodem_collecting = 0;
//最后一次往拼装数组里追加字节的时间
static uint32_t xmodem_stream_tick = 0;
IAP_status uart_iap;

//重置拼凑缓冲区
static void uart_iap_reset_stream_state(void)
{
    memset(xmodem_packet, 0, sizeof(xmodem_packet));
    xmodem_packet_len = 0;
    xmodem_collecting = 0;
    xmodem_stream_tick = 0;
}

static uint8_t strstr_hex(uint8_t *buf, uint16_t buf_len,
                          const char *sub, uint8_t sub_len)
{
    uint16_t i, j;

    if(sub_len > buf_len) return 0;

    for(i = 0; i <= buf_len - sub_len; i++) {
        for(j = 0; j < sub_len; j++) {
            if(buf[i + j] != (uint8_t)sub[j]) {
                break;
            }
        }
        if(j == sub_len) {
            return 1;
        }
    }

    return 0;
}

static void uart_iap_reset_transfer_state(void)
{
    memset(huancun, 0, sizeof(huancun));
    memset(write_info, 0, sizeof(write_info));
    memset(payload_queue, 0, sizeof(payload_queue));

    payload_queue_head = 0;
    payload_queue_tail = 0;
    payload_queue_count = 0;
    page_fill = 0;
    eot_pending = 0;
    uart_iap_reset_stream_state();

    frame_size = 0;
    next = 0;
    handshake_start = 0;
    last_packet_tick = 0;
    write_addr = GD25Q32_UPDATE_ADDR;
    erase_ahead_addr = GD25Q32_UPDATE_ADDR;
    expected_blk = 1;

    uart_iap.rx_packnum = 0;
}

static void uart_iap_enter_idle(void)
{
    uart_iap_reset_transfer_state();
    uart_iap.sta_flag = IAP_FLAG_IDLE;
}

//判断队列里面有空槽，还可以写入.缓冲区数据放入队列
static uint8_t uart_iap_queue_push(uint8_t *payload)
{   
    //如果当前队列里已经堆满了 10 个未处理包，就不允许再写新的包。
    if(payload_queue_count >= IAP_RAM_QUEUE_DEPTH) {
        return 0;
    }

    //有空槽
    memcpy(payload_queue[payload_queue_tail], payload, IAP_PACKET_DATA_LEN);
    payload_queue_tail++;
    if(payload_queue_tail >= IAP_RAM_QUEUE_DEPTH) {
        payload_queue_tail = 0;
    }
    payload_queue_count++;

    return 1;
}

//把队列某个数据包拷贝到huancun
static uint8_t uart_iap_queue_pop(uint8_t *payload)
{
    if(payload_queue_count == 0) {
        return 0;
    }

    memcpy(payload, payload_queue[payload_queue_head], IAP_PACKET_DATA_LEN);
    payload_queue_head++;
    if(payload_queue_head >= IAP_RAM_QUEUE_DEPTH) {
        payload_queue_head = 0;
    }
    payload_queue_count--;

    return 1;
}

static uint32_t uart_iap_buffered_bytes(void)
{
    return ((uint32_t)payload_queue_count * IAP_PACKET_DATA_LEN) + (uint32_t)page_fill;
}


//发应答位
static void uart_iap_accept_packet(void)
{
    next = get_tick();
    last_packet_tick = next;
    uart_iap.rx_packnum++;
    expected_blk++;
    uart0_send_byte(0x06);
    uart_iap.sta_flag = IAP_FLAG_RECEIVING;
}

//把拼凑缓冲区入队列+校验发送应答位
static void uart_iap_process_packet(void)
{
    if((xmodem_packet[1] + xmodem_packet[2]) != 0xff) {
        uart0_send_byte(0x15);
        return;
    }

    if(uart_iap.sta_flag == IAP_FLAG_HANDSHAKE) {
        if(xmodem_packet[1] == expected_blk) {
            if(Xmode_CRC16(xmodem_packet + 3, 128) == ((uint16_t)xmodem_packet[131] << 8 | xmodem_packet[132])) {
                if(uart_iap_queue_push(xmodem_packet + 3)) {
                    uart_iap_accept_packet();
                } else {
                    uart0_send_byte(0x15);
                }
            } else {
                uart0_send_byte(0x15);
            }
        } else {
            uart0_send_byte(0x15);
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
        if(xmodem_packet[1] == expected_blk) {
            if(Xmode_CRC16(xmodem_packet + 3, 128) == ((uint16_t)xmodem_packet[131] << 8 | xmodem_packet[132])) {
                if(uart_iap_queue_push(xmodem_packet + 3)) {
                    uart_iap_accept_packet();
                } else {
                    uart0_send_byte(0x15);
                }
            } else {
                uart0_send_byte(0x15);
            }
        } else if(xmodem_packet[1] == (uint8_t)(expected_blk - 1)) {
            last_packet_tick = get_tick();
            uart0_send_byte(0x06);//重发数据包
        } else {
            uart0_send_byte(0x15);
        }
    }
}


//流式组包
static void uart_iap_feed_stream(uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t resync_on_new_soh;

    //如果当前拼凑缓冲区有字节且重新接收的数据包以SOH开始，则重置接收状态
    resync_on_new_soh = (uint8_t)((xmodem_collecting != 0U) && (xmodem_packet_len > 0U) &&
                                  (len > 0U) && (data[0] == 0x01U));
    if(resync_on_new_soh != 0U) {
        uart_iap_reset_stream_state();
    }

    for(i = 0; i < len; i++) {
        if(xmodem_collecting == 0U) {
            if(data[i] == 0x01U) {
                xmodem_collecting = 1;
                xmodem_packet_len = 0;
                xmodem_packet[xmodem_packet_len++] = data[i];
                xmodem_stream_tick = get_tick();
            } else if((uart_iap.sta_flag == IAP_FLAG_RECEIVING) && (data[i] == 0x04U)) {
                last_packet_tick = get_tick();
                eot_pending = 1;
                xmodem_stream_tick = 0;
            } else if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
                uart0_send_byte(0x15);
            }
        } else {
            xmodem_packet[xmodem_packet_len++] = data[i];
            xmodem_stream_tick = get_tick();
            if(xmodem_packet_len >= sizeof(xmodem_packet)) {
                xmodem_collecting = 0;
                xmodem_packet_len = 0;
                xmodem_stream_tick = 0;
                uart_iap_process_packet();
            }
        }
    }
}

static uint8_t uart_iap_handle_stream_timeout(uint32_t now)
{
    if((xmodem_collecting != 0U) && (xmodem_stream_tick != 0U) &&
       ((now - xmodem_stream_tick) > IAP_STREAM_TIMEOUT_MS)) {
        uart_iap_reset_stream_state();
        if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
            uart0_send_byte(0x15);
        }
        return 1U;
    }

    return 0U;
}

//队列拷贝到huancun从huancun写gd25q32
static void uart_iap_storage_poll(void)
{
    uint32_t buffered_bytes;

    if((uart_iap.sta_flag != IAP_FLAG_RECEIVING) && (eot_pending == 0)) {
        return;
    }
    //把两个数据包合并成256
    while((page_fill + IAP_PACKET_DATA_LEN) <= IAP_PAGE_SIZE) {
        //队列里没有需要处理的数据包
        if(payload_queue_count == 0) {
            break;
        }
        //队列数据拷贝进huancun
        uart_iap_queue_pop(&huancun[page_fill]);
        page_fill += IAP_PACKET_DATA_LEN;

        if(page_fill == IAP_PAGE_SIZE) {
            break;
        }
    }

    buffered_bytes = uart_iap_buffered_bytes();
    //擦下一个扇区
    if((buffered_bytes > 0U) && (erase_ahead_addr <= (write_addr + buffered_bytes))) {
        gd25q32_erase_sector(erase_ahead_addr);
        erase_ahead_addr += GD25Q32_SECTOR_SIZE;
        return;
    }
    //攒够256字节写一页
    if(page_fill == IAP_PAGE_SIZE) {
        gd25q32_page_program(write_addr, huancun, IAP_PAGE_SIZE);
        write_addr += IAP_PAGE_SIZE;
        memset(huancun, 0, sizeof(huancun));
        page_fill = 0;
        return;
    }

    if(eot_pending != 0U) {
        //补足不足256字节写入
        if((payload_queue_count == 0U) && (page_fill > 0U)) {
            gd25q32_page_program(write_addr, huancun, page_fill);
            write_addr += page_fill;
            memset(huancun, 0, sizeof(huancun));
            page_fill = 0;
            return;
        }

        if((payload_queue_count == 0U) && (page_fill == 0U)) {
            uart0_send_byte(0x06);
            eot_pending = 0;
            uart_iap.sta_flag = IAP_FLAG_DONE;
        }
    }
}

void uart_iap_init(void)
{
    uart_iap_reset_transfer_state();

    uart_iap.delay_count = 0;
    uart_iap.delay_flag = 0;
    uart_iap.sta_flag = IAP_FLAG_IDLE;
}


//空闲中断结束后才会进入该函数
void uart0_rx_flush(void)
{
    __disable_irq();


    dma_channel_disable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);

    memset(URX_BUFF, 0, sizeof(URX_BUFF));

    urx.IN  = urx.URX_ptr;
    urx.OUT = urx.URX_ptr;
    urx.END = &urx.URX_ptr[URX_ptr_SIZE - 1];
    urx.rx_count = 0;

    urx.IN->start  = &URX_BUFF[0];
    urx.IN->end    = &URX_BUFF[0];
    urx.OUT->start = &URX_BUFF[0];
    urx.OUT->end   = &URX_BUFF[0];

    /* 清除 overflow 标志 */
    urx.overflow = 0;
    urx.overflow_count = 0;
    urx.dma_on_drop_buf = 0;

    //关闭DMA。在发送'C'前。不会出现数据在DMA关闭期间就传输过来
    dma_memory_address_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                              DMA_MEMORY_0, (uint32_t)&URX_BUFF[0]);
    dma_transfer_number_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, uart0_dma_buf_size);
    dma_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, DMA_FLAG_FTF);
    dma_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, DMA_FLAG_HTF);

    usart_dma_receive_config(DEBUG_UART, USART_RECEIVE_DMA_ENABLE);
    dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);

    __enable_irq();
}


void uart_iap_feed(uint8_t *data, uint16_t len)
{
    if(uart_iap.sta_flag == IAP_FLAG_IDLE) {
        if(strstr_hex(data, len, "update", 6)) {
            uart0_rx_flush();//每次收到upadate 清除缓冲区
            uart_iap_reset_transfer_state();
            uart_iap.sta_flag = IAP_FLAG_HANDSHAKE;
            handshake_start = get_tick();
            next = handshake_start;
            uart0_send_byte('C');
        }
    } else if((uart_iap.sta_flag == IAP_FLAG_HANDSHAKE) || (uart_iap.sta_flag == IAP_FLAG_RECEIVING)) {
        uart_iap_feed_stream(data, len);
    }
}

void uart_iap_poll(void)
{
    uint32_t now;

    if(uart_iap.sta_flag == IAP_FLAG_HANDSHAKE) {
        now = get_tick();
        if(uart_iap_handle_stream_timeout(now) != 0U) {
            next = now;
            uart0_send_byte('C');
        } else if(now - handshake_start > IAP_HANDSHAKE_TIMEOUT_MS) {
            uart0_rx_flush();
            uart_iap_enter_idle();
        } else if((xmodem_collecting == 0U) && (now - next > 1000U)) {
            uart0_send_byte('C');
            next = now;
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
        now = get_tick();
        if(uart_iap_handle_stream_timeout(now) == 0U) {
            uart_iap_storage_poll();
        }

        if(eot_pending == 0U) {
            if(now - last_packet_tick > IAP_RECEIVE_TIMEOUT_MS) {
                uart0_rx_flush();
                uart_iap_enter_idle();
            }
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_DONE) {
        gd25q32_erase_sector(GD25Q32_INFO_ADDR);
        write_info[0] = 0xaa;
        write_info[1] = 0xbb;
        write_info[2] = 0xcc;
        write_info[3] = 0xdd;
        frame_size = uart_iap.rx_packnum * 128;
        write_info[4] = (frame_size >> 24) & 0xff;
        write_info[5] = (frame_size >> 16) & 0xff;
        write_info[6] = (frame_size >> 8) & 0xff;
        write_info[7] = (frame_size) & 0xff;
        gd25q32_page_program(GD25Q32_INFO_ADDR, write_info, 8);
        delay_ms(100);
        NVIC_SystemReset();
    }
}

// /**
//  * @brief CRC16校验
//  * @param data 起始数据指针
//  * @param len 数据长度
//  * @note
//  */



// const unsigned char TabH[] = {  //CRC高位字节值表
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
//         0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
//         0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
//         0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
//         0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
//         0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
//         0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
//         0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
//         0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
//         0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
//         0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
//         0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
//         0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
//         0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
//         0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
//     } ;
// const unsigned char TabL[] = {  //CRC低位字节值表
//         0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
//         0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
//         0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
//         0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
//         0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
//         0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
//         0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
//         0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
//         0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
//         0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
//         0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
//         0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
//         0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
//         0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
//         0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
//         0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
//         0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
//         0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
//         0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
//         0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
//         0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
//         0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
//         0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
//         0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
//         0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
//         0x43, 0x83, 0x41, 0x81, 0x80, 0x40
//     } ;

// /*************************************************************************************
//  * 函数说明: CRC16校验
//  * 入口参数：u8 *ptr,u8 len
//  * 出口参数：u16
//  * 函数功能：根据入口参数数组的值计算crc16校验值 并返回
// **************************************************************************************/
// uint16_t GetCRC16(unsigned char *data,unsigned char len)
// {
//     unsigned int  uiIndex;
//     unsigned char ucCrch = 0x00;          // 初始化CRC高字节为0x00
//     unsigned char ucCrcl = 0x00;          // 初始化CRC低字节为0x00
//     while (len --)          //循环处理每个数据字节（共ucLen个）
//     {
//         // 1. 当前数据字节与CRC高字节异或，得到查表索引
//         uiIndex = ucCrch ^ *data++;       // pPtr指向当前数据字节，处理后指针自增
//         // 2. 通过CRC表更新高、低字节（快速计算多项式运算结果）
//         ucCrch  = ucCrcl ^ TabH[uiIndex]; // 高字节 = 原低字节 异或 表中高字节值
//         ucCrcl  = TabL[uiIndex];          // 低字节 = 表中低字节值

//     }
//      // 3. 将高、低字节组合为16位整数返回（高字节左移8位 + 低字节）
//     return ((ucCrch << 8) | ucCrcl);
// }


uint16_t Xmode_CRC16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0x0000;
    uint16_t poly = 0x1021;
    uint16_t i, j;

    for(i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);

        for(j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}


