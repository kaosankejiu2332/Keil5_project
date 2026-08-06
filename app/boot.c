#include "boot.h"
#include "my_uart.h"
#include "my_spi.h"
#include "my_gd25q32.h"
#include "my_flash.h"
#include <stdio.h>

static uint8_t boot_info[8];

uint8_t app_vector_is_valid(uint32_t app_addr)
{
    uint32_t initial_msp = *(volatile uint32_t *)app_addr;
    uint32_t reset_vector = *(volatile uint32_t *)(app_addr + 4U);
    uint32_t reset_address = reset_vector & ~1UL;
    uint8_t msp_valid;

    msp_valid = (uint8_t)((((initial_msp >= 0x20000000UL) &&
                            (initial_msp <= 0x20030000UL)) ||
                           ((initial_msp >= 0x10000000UL) &&
                            (initial_msp <= 0x10010000UL))) &&
                          ((initial_msp & 0x7U) == 0U));

    if((msp_valid == 0U) ||
       ((reset_vector & 1U) == 0U) ||
       (reset_address < APP_START_ADDR) ||
       (reset_address >= APP_END_ADDR)) {
        return 0U;
    }
    return 1U;
}

void boot_clear(void)
{
    uint32_t index;

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    usart_dma_receive_config(DEBUG_UART, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    dma_deinit(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    usart_deinit(DEBUG_UART);
    spi_i2s_deinit(FLASH_SPI);

    for(index = 0U; index < 8U; index++) {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }
}

void jump_to_app(uint32_t app_addr)
{
    app_func_t app_entry;
    uint32_t initial_msp;

    if(app_vector_is_valid(app_addr) == 0U) {
        printf("APP vector invalid\r\n");
        return;
    }

    initial_msp = *(volatile uint32_t *)app_addr;
    app_entry = (app_func_t)*(volatile uint32_t *)(app_addr + 4U);

    __disable_irq();
    boot_clear();
    SCB->VTOR = app_addr;
    __set_CONTROL(0U);
    __set_PSP(0U);
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);
    __DSB();
    __ISB();
    __set_MSP(initial_msp);
    __enable_irq();
    app_entry();
}

void boot_branches(void)
{
    uint32_t flag;
    uint32_t firmware_size;

    gd25q32_read_data(GD25Q32_INFO_ADDR, boot_info, sizeof(boot_info));
    flag = ((uint32_t)boot_info[0] << 24)
           | ((uint32_t)boot_info[1] << 16)
           | ((uint32_t)boot_info[2] << 8)
           | (uint32_t)boot_info[3];
    firmware_size = ((uint32_t)boot_info[4] << 24)
                    | ((uint32_t)boot_info[5] << 16)
                    | ((uint32_t)boot_info[6] << 8)
                    | (uint32_t)boot_info[7];

    if(flag == BOOT_UPDATE_MAGIC) {
        if((firmware_size == 0U) || (firmware_size > APP_MAX_SIZE)) {
            printf("Firmware size invalid\r\n");
            if(gd25q32_erase_sector(GD25Q32_INFO_ADDR) == 0U) {
                return;
            }
            jump_to_app(APP_START_ADDR);
            return;
        }

        fmc_unlock();
        if((flash_sector_erase(firmware_size) == 0U) ||
           (flash_update_data(firmware_size) == 0U)) {
            fmc_lock();
            printf("Internal flash update failed\r\n");
            return;
        }
        fmc_lock();

        if(app_vector_is_valid(APP_START_ADDR) == 0U) {
            printf("Updated APP vector invalid\r\n");
            return;
        }

        if(gd25q32_erase_sector(GD25Q32_INFO_ADDR) == 0U) {
            printf("Update marker clear failed\r\n");
            return;
        }
        NVIC_SystemReset();
    }

    jump_to_app(APP_START_ADDR);
}
