#ifndef __MY_SPI_H__
#define __MY_SPI_H__

#include "gd32f4xx.h"

/* ©¤©¤ SPI Flash ½Ó¿Ú ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
#define FLASH_SPI               SPI0
#define FLASH_SPI_RCU           RCU_SPI0

#define FLASH_SPI_GPIO_PORT     GPIOB           /* SCK / MISO / MOSI */
#define FLASH_SPI_GPIO_RCU      RCU_GPIOB
#define FLASH_SPI_AF            GPIO_AF_5
#define FLASH_SPI_SCK_PIN       GPIO_PIN_3
#define FLASH_SPI_MISO_PIN      GPIO_PIN_4
#define FLASH_SPI_MOSI_PIN      GPIO_PIN_5

#define FLASH_NSS_GPIO_PORT     GPIOA           /* NSS (Èí¼þ CS) */
#define FLASH_NSS_GPIO_RCU      RCU_GPIOA
#define FLASH_NSS_PIN           GPIO_PIN_15


/* SPI Flash */
void spi1_init(void);
uint8_t spi1_read_write_byte(uint8_t byte);

#endif


