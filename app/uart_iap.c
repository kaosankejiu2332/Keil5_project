#include "uart_iap.h"
#include "systick.h"
#include "my_uart.h"
#include "my_gd25q32.h"
#include <string.h>

static uint8_t huancun[260] = {0};
static uint8_t write_info[8] = {0};
static uint32_t frame_size = 0;
static uint32_t next = 0;
static uint32_t handshake_start = 0;
static uint32_t last_packet_tick = 0;
static uint32_t write_addr = GD25Q32_UPDATE_ADDR;
static uint8_t expected_blk = 1;
IAP_status uart_iap;

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

void uart_iap_init(void)
{
    memset(huancun, 0, sizeof(huancun));
    memset(write_info, 0, sizeof(write_info));
    frame_size = 0;
    next = 0;
    handshake_start = 0;
    last_packet_tick = 0;
    write_addr = GD25Q32_UPDATE_ADDR;
    expected_blk = 1;

    uart_iap.delay_count = 0;
    uart_iap.delay_flag = 0;
    uart_iap.sta_flag = IAP_FLAG_IDLE;
    uart_iap.rx_packnum = 0;
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

    //关闭DMA在发送'C'前。不会出现数据在DMA关闭期间就传输过来
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
            uart_iap.sta_flag = IAP_FLAG_HANDSHAKE;
            uart_iap.rx_packnum = 0;
            write_addr = GD25Q32_UPDATE_ADDR;
            memset(huancun, 0, sizeof(huancun));
            expected_blk = 1;
            handshake_start = get_tick();
            next = handshake_start;
            last_packet_tick = 0;
            uart0_send_byte('C');
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_HANDSHAKE) {
        if(*data == 0x01) {
            if((len == 133) && (data[1] + data[2] == 0xff)) {
                if(data[1] == expected_blk) {
                    if(Xmode_CRC16(data + 3, 128) == ((uint16_t)data[131] << 8 | data[132])) {
                        uart_iap.rx_packnum++;
                        memcpy(&huancun[(uart_iap.rx_packnum & 1) ? 0 : 128], data + 3, 128);
                        if(uart_iap.rx_packnum % 2 == 0) {
                            if((write_addr % GD25Q32_SECTOR_SIZE) == 0) {
                                gd25q32_erase_sector(write_addr);
                            }
                            gd25q32_page_program(write_addr, huancun, 256);
                            write_addr += 256;
                            memset(huancun, 0, sizeof(huancun));
                        }
                        next = get_tick();
                        last_packet_tick = next;
                        expected_blk++;
                        uart0_send_byte(0x06);
                        uart_iap.sta_flag = IAP_FLAG_RECEIVING;
                    } else {
                        uart0_send_byte(0x15);
                    }
                } else {
                    uart0_send_byte(0x15);
                }
            } else {
                uart0_send_byte(0x15);
            }
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
        if(*data == 0x01) {
            if((len == 133) && (data[1] + data[2] == 0xff)) {
                if(data[1] == expected_blk) {
                    if(Xmode_CRC16(data + 3, 128) == ((uint16_t)data[131] << 8 | data[132])) {
                        uart_iap.rx_packnum++;
                        memcpy(&huancun[(uart_iap.rx_packnum & 1) ? 0 : 128], data + 3, 128);
                        if(uart_iap.rx_packnum % 2 == 0) {
                            if((write_addr % GD25Q32_SECTOR_SIZE) == 0) {
                                gd25q32_erase_sector(write_addr);
                            }
                            gd25q32_page_program(write_addr, huancun, 256);
                            write_addr += 256;
                            memset(huancun, 0, sizeof(huancun));
                        }
                        next = get_tick();
                        last_packet_tick = next;
                        expected_blk++;
                        uart0_send_byte(0x06);
                    } else {
                        uart0_send_byte(0x15);
                    }
                } else if(data[1] == (uint8_t)(expected_blk - 1)) {
                    last_packet_tick = get_tick();
                    uart0_send_byte(0x06);
                } else {
                    uart0_send_byte(0x15);
                }
            } else {
                uart0_send_byte(0x15);
            }
        } else if(*data == 0x04) {
            if(uart_iap.rx_packnum % 2 == 1) {
                if((write_addr % GD25Q32_SECTOR_SIZE) == 0) {
                    gd25q32_erase_sector(write_addr);
                }
                gd25q32_page_program(write_addr, huancun, 128);
                write_addr += 128;
            }
            uart0_send_byte(0x06);
            uart_iap.sta_flag = IAP_FLAG_DONE;
        } else {
            uart0_send_byte(0x15);
        }
    }
}

void uart_iap_poll(void)
{
    uint32_t now;

    /* 检查 UART 接收层 overflow */
    if(uart0_rx_is_overflow()) {
        uart0_rx_clear_overflow();
        uart0_rx_flush();

        uart_iap.sta_flag = IAP_FLAG_IDLE;
        uart_iap.rx_packnum = 0;
        expected_blk = 1;
        last_packet_tick = 0;
        next = 0;
        handshake_start = 0;
        write_addr = GD25Q32_UPDATE_ADDR;
        memset(huancun, 0, sizeof(huancun));

        uart0_send_byte(0x15);  // 回 NAK 通知对端
        return;
    }

    if(uart_iap.sta_flag == IAP_FLAG_HANDSHAKE) {
        now = get_tick();
        if(now - handshake_start > 15000) {
            uart_iap.sta_flag = IAP_FLAG_IDLE;
            uart0_rx_flush();
            uart_iap.rx_packnum = 0;
            expected_blk = 1;
            last_packet_tick = 0;
            next = 0;
            handshake_start = 0;
            write_addr = GD25Q32_UPDATE_ADDR;
            memset(huancun, 0, sizeof(huancun));
        } else if(now - next > 1000) {
            uart0_send_byte('C');
            next = now;
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
        now = get_tick();
        if(now - last_packet_tick > 3000) {
            uart_iap.sta_flag = IAP_FLAG_IDLE;
            uart0_rx_flush();
            uart_iap.rx_packnum = 0;
            // uart_iap.erase_flag = 0;
            expected_blk = 1;
            last_packet_tick = 0;
            next = 0;
            handshake_start = 0;
            write_addr = GD25Q32_UPDATE_ADDR;
            memset(huancun, 0, sizeof(huancun));
        }
    } else if(uart_iap.sta_flag == IAP_FLAG_DONE) {
        // uart_iap.erase_flag = 0;
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

/**
 * @brief CRC16校验
 * @param data 起始数据指针
 * @param len 数据长度
 * @note
 */



const unsigned char TabH[] = {  //CRC高位字节值表
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,  
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,  
        0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,  
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,  
        0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,  
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,  
        0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,  
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,  
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,  
        0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,  
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,  
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,  
        0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,  
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,  
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,  
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,  
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,  
        0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,  
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,  
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40  
    } ;  
const unsigned char TabL[] = {  //CRC低位字节值表
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,  
        0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,  
        0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,  
        0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,  
        0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,  
        0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,  
        0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,  
        0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,  
        0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,  
        0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,  
        0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,  
        0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,  
        0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,  
        0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,  
        0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,  
        0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,  
        0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,  
        0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,  
        0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,  
        0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,  
        0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,  
        0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,  
        0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,  
        0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,  
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,  
        0x43, 0x83, 0x41, 0x81, 0x80, 0x40  
    } ;

/*************************************************************************************
 * 函数说明: CRC16校验
 * 入口参数：u8 *ptr,u8 len
 * 出口参数：u16
 * 函数功能：根据入口参数数组的值计算crc16校验值 并返回
**************************************************************************************/
unsigned int GetCRC16(unsigned char *pPtr,unsigned char ucLen)
{ 
    unsigned int  uiIndex;
    unsigned char ucCrch = 0xFF;  		// 初始化CRC高字节为0xFF
    unsigned char ucCrcl = 0xFF;  		// 初始化CRC低字节为0xFF 
    while (ucLen --)  			//循环处理每个数据字节（共ucLen个）
    {
        // 1. 当前数据字节与CRC高字节异或，得到查表索引
        uiIndex = ucCrch ^ *pPtr++;       // pPtr指向当前数据字节，处理后指针自增
        // 2. 通过CRC表更新高、低字节（快速计算多项式运算结果）
        ucCrch  = ucCrcl ^ TabH[uiIndex]; // 高字节 = 原低字节 异或 表中高字节值
        ucCrcl  = TabL[uiIndex];          // 低字节 = 表中低字节值
    
    }
     // 3. 将高、低字节组合为16位整数返回（高字节左移8位 + 低字节）
    return ((ucCrch << 8) | ucCrcl);  
} 



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
