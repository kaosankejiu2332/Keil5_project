#include "my_gd25q32.h"
#include "my_spi.h"

#define GD25Q32_BUSY_POLL_LIMIT  20000000UL

/* NSS control (引脚宏定义在 user_gpio.h 中) */
static void gd25q32_cs_low(void)
{
    gpio_bit_reset(FLASH_NSS_GPIO_PORT, FLASH_NSS_PIN);
}

static void gd25q32_cs_high(void)
{
    gpio_bit_set(FLASH_NSS_GPIO_PORT, FLASH_NSS_PIN);
}

/*
 * 读取 Flash JEDEC ID (命令 0x9F)
 * 返回: 32 位 ID
 *       [23:16] = Manufacturer ID (GigaDevice = 0xC8)
 *       [15:8]  = Memory Type     (GD25Q32 = 0x40)
 *       [7:0]   = Capacity ID     (GD25Q32 = 0x16)
 */
uint32_t gd25q32_read_jedec_id(void)
{
    uint32_t id = 0;

    gd25q32_cs_low();

    spi1_read_write_byte(FLASH_CMD_READ_JEDEC_ID);

    id |= (uint32_t)spi1_read_write_byte(0xFF) << 16;
    id |= (uint32_t)spi1_read_write_byte(0xFF) << 8;
    id |= (uint32_t)spi1_read_write_byte(0xFF);

    gd25q32_cs_high();

    return id;
}

/* 写使能 (命令 0x06) */
static uint8_t gd25q32_read_status1(void)
{
    uint8_t status;

    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_READ_STATUS_REG1);
    status = spi1_read_write_byte(0xFF);
    gd25q32_cs_high();
    return status;
}

uint8_t gd25q32_write_enable(void)
{
    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_WRITE_ENABLE);
    gd25q32_cs_high();
    return (uint8_t)((gd25q32_read_status1() & FLASH_SR1_WEL) != 0U);
}

/* 等待忙状态结束 (轮询 SR1 的 WIP 位, 命令 0x05) */
uint8_t gd25q32_wait_busy(void)
{
    uint32_t poll_count;

    for(poll_count = 0U; poll_count < GD25Q32_BUSY_POLL_LIMIT; poll_count++) {
        if((gd25q32_read_status1() & FLASH_SR1_WIP) == 0U) {
            return 1U;
        }
    }
    return 0U;
}

/* 擦除 4KB 扇区 (命令 0x20)
 * addr: 字节地址 (0x000000 ~ 0x0FFFFF)
 */
uint8_t gd25q32_erase_sector(uint32_t addr)
{
    if((addr >= GD25Q32_FLASH_SIZE) ||
       (gd25q32_wait_busy() == 0U) ||
       (gd25q32_write_enable() == 0U)) {
        return 0U;
    }

    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_SECTOR_ERASE);
    spi1_read_write_byte((uint8_t)(addr >> 16));
    spi1_read_write_byte((uint8_t)(addr >> 8));
    spi1_read_write_byte((uint8_t)addr);
    gd25q32_cs_high();

    return gd25q32_wait_busy();
}


/* 擦除 64KB 块 (命令 0xD8) */
uint8_t gd25q32_erase_block_64k(uint32_t addr)
{
    if((addr >= GD25Q32_FLASH_SIZE) ||
       (gd25q32_wait_busy() == 0U) ||
       (gd25q32_write_enable() == 0U)) {
        return 0U;
    }

    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_BLOCK_ERASE_64K);
    spi1_read_write_byte((uint8_t)(addr >> 16));
    spi1_read_write_byte((uint8_t)(addr >> 8));
    spi1_read_write_byte((uint8_t)addr);
    gd25q32_cs_high();

    return gd25q32_wait_busy();
}


/* 整片擦除 (命令 0xC7)
 * 注意: 耗时较长 (典型 40s), 调用后需等待 busy 完成
 */
uint8_t gd25q32_erase_chip(void)
{
    if((gd25q32_wait_busy() == 0U) ||
       (gd25q32_write_enable() == 0U)) {
        return 0U;
    }

    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_CHIP_ERASE);
    gd25q32_cs_high();

    return gd25q32_wait_busy();
}

/* 页编程 (命令 0x02), 单次最多 256 字节
 * addr: 字节地址
 * data: 数据指针
 * len:  写入长度 (≤ 256 字节, 不能跨页边界)
 */
uint8_t gd25q32_page_program(uint32_t addr, const uint8_t *data, uint16_t len)
{   
    uint16_t i ;
    if((data == 0) || (len == 0U) || (len > GD25Q32_PAGE_SIZE) ||
       ((addr + len) > GD25Q32_FLASH_SIZE) ||
       (((addr & (GD25Q32_PAGE_SIZE - 1U)) + len) > GD25Q32_PAGE_SIZE) ||
       (gd25q32_wait_busy() == 0U) ||
       (gd25q32_write_enable() == 0U)) {
        return 0U;
    }

    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_PAGE_PROGRAM);
    spi1_read_write_byte((uint8_t)(addr >> 16));
    spi1_read_write_byte((uint8_t)(addr >> 8));
    spi1_read_write_byte((uint8_t)addr);
    
    for ( i = 0; i < len; i++)
    {

    spi1_read_write_byte(data[i]);

    }
    gd25q32_cs_high();
    return gd25q32_wait_busy();
}

/* 读取数据 (命令 0x03)
 * addr: 字节地址
 * data: 接收缓冲区指针
 * len:  读取长度
 */
void gd25q32_read_data(uint32_t addr, uint8_t *data, uint16_t len)
{   
    uint16_t i ;
    gd25q32_cs_low();
    spi1_read_write_byte(FLASH_CMD_READ_DATA);
    spi1_read_write_byte((uint8_t)(addr >> 16));
    spi1_read_write_byte((uint8_t)(addr >> 8));
    spi1_read_write_byte((uint8_t)addr);

    for (i = 0; i < len; i++) {
        data[i] = spi1_read_write_byte(0xFF);
    }
    gd25q32_cs_high();
}


// typedef void (*app_func_t)(void);//定义一个指向无返回，无参数函数的指针类型
// app_func_t app_entry;//定义一个函数指针变量


// void jump_to_app(uint32_t app_addr)
// { 
//     if((*(volatile uint32_t *)app_addr)&0x2FF00000==0x20000000)
//     {
//         //APP的栈顶指针赋值给MSP寄存器
//         __set_MSP((*(volatile uint32_t *)app_addr));//__set_MSP是内置函数，
//         //参数填栈顶指针
//         //复位向量
//         app_entry=(app_func_t)(*(volatile uint32_t *)app_addr+4);
//         app_entry();//执行函数
//     }else
//     {
//         printf("ERROR\n");
//     }

// }
