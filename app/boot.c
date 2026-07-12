#include "boot.h"
#include "my_uart.h"
#include "my_spi.h"
#include <stdio.h>
#include "my_gd25q32.h"
#include "my_flash.h"


/**
 * @brief B区的外设全部初始化供A区使用
 */
void boot_clear(void)
{
    usart_deinit(DEBUG_UART);
    dma_deinit(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    usart_interrupt_disable(DEBUG_UART, USART_INT_IDLE);
    spi_i2s_deinit(FLASH_SPI);
}




app_func_t app_entry;//定义一个函数指针变量
/**
 * @brief 跳转到用户应用程序执行
 * @param app_addr 用户应用程序的起始地址
 * @note 该函数用于从引导程序跳转到用户应用程序执行
 */
void jump_to_app(uint32_t app_addr)
{ 
    if(((*(volatile uint32_t *)app_addr)&0x2FF00000)==0x20000000)
    {
        //APP的栈顶指针赋值给MSP寄存器
        __set_MSP((*(volatile uint32_t *)app_addr));//__set_MSP是内置函数，
        //参数填值
         //复位向量
        app_entry=(app_func_t)(*(volatile uint32_t *)(app_addr+4));
        app_entry();//执行复位向量对应的函数

    }else
    {
        printf("ERROR\n");
    }

}

uint8_t info[10]={0};
uint32_t flag=0;
uint32_t firmware_size=0;


/**
 * @brief 判断OTA标志
 *
 * 
 */
void boot_branches(void)
{
    gd25q32_read_data(GD25Q32_INFO_ADDR, info, 8);
    flag=(info[0]<<24 | info[1]<<16 | info[2]<<8 | info[3]);//aabbccdd
    firmware_size=info[4]<<24 | info[5]<<16 | info[6]<<8 | info[7];
    printf("=== Boot Start ===\r\n");

        if(flag==0xaabbccdd)
    {   
        //更新A区程序
        fmc_unlock();
        flash_sector_erase();
        flash_update_data(firmware_size);
        fmc_lock();
        gd25q32_erase_sector(GD25Q32_INFO_ADDR);//擦除标志位
        NVIC_SystemReset();
    }
    else
    {
        printf("Bootloader: Jumping to application...\r\n");
        boot_clear();
        jump_to_app(APP_START_ADDR);
    }

}


