#include "uart_iap.h"
#include "systick.h"
#include "my_uart.h"
#include "my_gd25q32.h"
#include <string.h>

#define IAP_PAGE_SIZE             256U
#define IAP_COMMAND_BUF_SIZE       64U
#define IAP_UPDATE_PREFIX          "<UPDATE,"
#define IAP_UPDATE_PREFIX_LEN       8U
#define IAP_UPDATE_MAGIC   0xAABBCCDDUL
#define IAP_RESET_DELAY_MS         100U

static uint8_t page_buf[IAP_PAGE_SIZE];
static uint16_t page_fill;
static uint32_t write_addr;
static uint32_t done_tick;

static char command_buf[IAP_COMMAND_BUF_SIZE];
static uint8_t command_len;
static uint8_t command_collecting;

IAP_status uart_iap;

static void uart_iap_reset_command(void)
{
    command_len = 0U;
    command_collecting = 0U;
    memset(command_buf, 0, sizeof(command_buf));
}

static void uart_iap_reset_transfer(void)
{
    memset(page_buf, 0xFF, sizeof(page_buf));
    page_fill = 0U;
    write_addr = GD25Q32_UPDATE_ADDR;
    done_tick = 0U;
    uart_iap.expected_size = 0U;
    uart_iap.received_size = 0U;
    uart_iap.written_size = 0U;
}

static const char *uart_iap_error_text(iap_error_t error)
{
    switch(error) {
    case IAP_ERROR_COMMAND:
        return "<CMD_ERR>";
    case IAP_ERROR_SIZE:
        return "<SIZE_ERR>";
    case IAP_ERROR_OVERFLOW:
        return "<OVERFLOW>";
    case IAP_ERROR_UART:
        return "<UART_ERR>";
    case IAP_ERROR_FLASH:
        return "<FLASH_ERR>";
    default:
        return "<ERROR>";
    }
}

void uart_iap_abort(iap_error_t error)
{
    if((uart_iap.sta_flag == IAP_FLAG_DONE) ||
       (uart_iap.sta_flag == IAP_FLAG_ERROR)) {
        return;
    }

    uart_iap.error = (uint8_t)error;
    uart_iap.sta_flag = IAP_FLAG_ERROR;
    uart0_send_string(uart_iap_error_text(error));
    uart0_rx_flush();
    uart_iap_reset_command();
}

/*解析字符串 是否以 "<UPDATE," 开头结尾是否是 >中间是不是合法数字*/
static uint8_t uart_iap_parse_update_size(uint32_t *size)
{
    uint32_t value = 0U;
    uint8_t index;
    uint8_t digit_count = 0U;

    if((command_len < (IAP_UPDATE_PREFIX_LEN + 2U)) ||
       (memcmp(command_buf, IAP_UPDATE_PREFIX, IAP_UPDATE_PREFIX_LEN) != 0) ||
       (command_buf[command_len - 1U] != '>')) {
        return 0U;
    }

    for(index = IAP_UPDATE_PREFIX_LEN;
        index < (uint8_t)(command_len - 1U);
        index++) {
        uint8_t digit;

        if((command_buf[index] < '0') || (command_buf[index] > '9')) {
            return 0U;
        }

        digit = (uint8_t)(command_buf[index] - '0');
        if(value > ((0xFFFFFFFFUL - digit) / 10UL)) {
            return 0U;
        }
        value = (value * 10UL) + digit;
        digit_count++;
    }

    if(digit_count == 0U) {
        return 0U;
    }

    *size = value;
    return 1U;
}

static void uart_iap_prepare(uint32_t firmware_size)
{
    uint32_t erase_addr;
    uint32_t erase_end;
    uint32_t jedec_id;

    if((firmware_size == 0U) ||
       (firmware_size > IAP_APP_MAX_SIZE) ||
       (firmware_size > GD25Q32_SIZE_MAX)) {
        uart_iap_abort(IAP_ERROR_SIZE);
        return;
    }

    uart_iap_reset_transfer();
    uart_iap.expected_size = firmware_size;
    uart_iap.error = IAP_ERROR_NONE;
    uart_iap.sta_flag = IAP_FLAG_PREPARING;

    jedec_id = gd25q32_read_jedec_id();
    if(jedec_id != (((uint32_t)GD25Q32_MANUFACTURER_ID << 16) |
                    ((uint32_t)GD25Q32_MEMORY_TYPE << 8) |
                    (uint32_t)GD25Q32_CAPACITY_ID)) {
        uart_iap_abort(IAP_ERROR_FLASH);
        return;
    }

    /* Remove the previous valid marker before accepting new firmware. */
    if(gd25q32_erase_sector(GD25Q32_INFO_ADDR) == 0U) {
        uart_iap_abort(IAP_ERROR_FLASH);
        return;
    }

    erase_end = GD25Q32_UPDATE_ADDR
                + ((firmware_size + GD25Q32_SECTOR_SIZE - 1U)
                   / GD25Q32_SECTOR_SIZE) * GD25Q32_SECTOR_SIZE;
    erase_addr = GD25Q32_UPDATE_ADDR;
    while((erase_addr + GD25Q32_BLOCK_SIZE_64K) <= erase_end) {
        if(gd25q32_erase_block_64k(erase_addr) == 0U) {
            uart_iap_abort(IAP_ERROR_FLASH);
            return;
        }
        erase_addr += GD25Q32_BLOCK_SIZE_64K;
    }
    for(; erase_addr < erase_end; erase_addr += GD25Q32_SECTOR_SIZE) {
        if(gd25q32_erase_sector(erase_addr) == 0U) {
            uart_iap_abort(IAP_ERROR_FLASH);
            return;
        }
    }

    /* Bytes sent before <OK> are deliberately discarded. */
    uart0_rx_flush();
    uart_iap.sta_flag = IAP_FLAG_RECEIVING;
    uart0_send_string("<OK>");
}

static void uart_iap_finish(void)
{
    uint8_t size_data[4];
    uint8_t magic_data[4];
    uint32_t size;

    if(page_fill > 0U) {
        if(gd25q32_page_program(write_addr, page_buf, page_fill) == 0U) {
            uart_iap_abort(IAP_ERROR_FLASH);
            return;
        }
        uart_iap.written_size += page_fill;
        write_addr += page_fill;
        page_fill = 0U;
    }

    if(uart_iap.written_size != uart_iap.expected_size) {
        uart_iap_abort(IAP_ERROR_FLASH);
        return;
    }

    uart_iap.sta_flag = IAP_FLAG_VERIFYING;
    size = uart_iap.expected_size;
    size_data[0] = (uint8_t)(size >> 24);
    size_data[1] = (uint8_t)(size >> 16);
    size_data[2] = (uint8_t)(size >> 8);
    size_data[3] = (uint8_t)size;

    magic_data[0] = (uint8_t)(IAP_UPDATE_MAGIC >> 24);
    magic_data[1] = (uint8_t)(IAP_UPDATE_MAGIC >> 16);
    magic_data[2] = (uint8_t)(IAP_UPDATE_MAGIC >> 8);
    magic_data[3] = (uint8_t)IAP_UPDATE_MAGIC;

    /* Size is written first; magic is the final commit marker. */
    if((gd25q32_page_program(GD25Q32_INFO_ADDR + 4U,
                             size_data, 4U) == 0U) ||
       (gd25q32_page_program(GD25Q32_INFO_ADDR,
                             magic_data, 4U) == 0U)) {
        uart_iap_abort(IAP_ERROR_FLASH);
        return;
    }

    uart_iap.sta_flag = IAP_FLAG_DONE;
    done_tick = get_tick();
    // uart0_send_string("<DONE>");
}


/*把循环缓冲区数据拷贝到页缓冲 页缓冲满256写入flash*/
static void uart_iap_receive_firmware(const uint8_t *data, uint32_t len)
{
    uint32_t remaining;

    remaining = uart_iap.expected_size - uart_iap.received_size;
    if(len > remaining) {
        uart_iap_abort(IAP_ERROR_SIZE);
        return;
    }

    while(len > 0U) {
        uint32_t space = IAP_PAGE_SIZE - page_fill;
        uint32_t copy_len = (len < space) ? len : space;

        memcpy(&page_buf[page_fill], data, copy_len);
        page_fill = (uint16_t)(page_fill + copy_len);
        uart_iap.received_size += copy_len;
        data += copy_len;
        len -= copy_len;

        if(page_fill == IAP_PAGE_SIZE) {
            if(gd25q32_page_program(write_addr, page_buf,
                                    IAP_PAGE_SIZE) == 0U) {
                uart_iap_abort(IAP_ERROR_FLASH);
                return;
            }
            write_addr += IAP_PAGE_SIZE;
            uart_iap.written_size += IAP_PAGE_SIZE;
            page_fill = 0U;
            memset(page_buf, 0xFF, sizeof(page_buf));
        }
    }

    if(uart_iap.received_size == uart_iap.expected_size) {
        uart_iap_finish();
    }
}

/*IDLE阶段收集< >里面的字符串*/
static void uart_iap_feed_command_byte(uint8_t byte)
{
    uint32_t firmware_size;

    if(byte == '<') {
        uart_iap_reset_command();
        command_collecting = 1U;
    }

    if(command_collecting == 0U) {
        return;
    }

    if(command_len >= (IAP_COMMAND_BUF_SIZE - 1U)) {
        uart_iap_reset_command();
        uart_iap_abort(IAP_ERROR_COMMAND);
        return;
    }

    command_buf[command_len++] = (char)byte;
    command_buf[command_len] = '\0';

    if(byte == '>') {
        command_collecting = 0U;
        //解析到合法的<UPDATE,size> 返回1
        if(uart_iap_parse_update_size(&firmware_size) == 0U) {
            uart_iap_abort(IAP_ERROR_COMMAND);
            return;
        }
        uart_iap_prepare(firmware_size);
        uart_iap_reset_command();
    }
}

void uart_iap_init(void)
{
    memset(&uart_iap, 0, sizeof(uart_iap));
    uart_iap_reset_transfer();
    uart_iap_reset_command();
    uart_iap.sta_flag = IAP_FLAG_IDLE;
}

void uart_iap_feed(const uint8_t *data, uint32_t len)
{
    uint32_t index;

    if((data == 0) || (len == 0U)) {
        return;
    }

    if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
        uart_iap_receive_firmware(data, len);
        return;
    }


    //IDLE状态解析<UPDATE,size>
    if((uart_iap.sta_flag == IAP_FLAG_IDLE) ||
       (uart_iap.sta_flag == IAP_FLAG_ERROR)) {
        for(index = 0U; index < len; index++) {
            uart_iap_feed_command_byte(data[index]);
            if(uart_iap.sta_flag == IAP_FLAG_RECEIVING) {
                /* Firmware must start only after the newly transmitted <OK>. */
                break;
            }
        }
    }
}

void uart_iap_poll(void)
{
    uint32_t now = get_tick();

    if(uart_iap.sta_flag == IAP_FLAG_DONE) {
        if((now - done_tick) >= IAP_RESET_DELAY_MS) {
            NVIC_SystemReset();
        }
    }
}
