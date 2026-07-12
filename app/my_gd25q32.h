#ifndef __MY_GD25Q32_H_
#define __MY_GD25Q32_H_

#include <gd32f4xx.h>
#include <gd32f4xx_spi.h>

/* Flash commands (GD25Q32CS1G / W25Qxx compatible) */
#define FLASH_CMD_WRITE_ENABLE        0x06   /* Write Enable */
#define FLASH_CMD_WRITE_DISABLE       0x04   /* Write Disable */
#define FLASH_CMD_READ_STATUS_REG1    0x05   /* Read Status Register 1 */
#define FLASH_CMD_READ_STATUS_REG2    0x35   /* Read Status Register 2 */
#define FLASH_CMD_READ_STATUS_REG3    0x15   /* Read Status Register 3 */
#define FLASH_CMD_WRITE_STATUS_REG    0x01   /* Write Status Register */
#define FLASH_CMD_SECTOR_ERASE        0x20   /* 4KB Sector Erase */
#define FLASH_CMD_BLOCK_ERASE_32K     0x52   /* 32KB Block Erase */
#define FLASH_CMD_BLOCK_ERASE_64K     0xD8   /* 64KB Block Erase */
#define FLASH_CMD_CHIP_ERASE          0xC7   /* Chip Erase */
#define FLASH_CMD_PAGE_PROGRAM        0x02   /* Page Program (max 256 bytes) */
#define FLASH_CMD_READ_DATA           0x03   /* Read Data */
#define FLASH_CMD_FAST_READ           0x0B   /* Fast Read */
#define FLASH_CMD_READ_JEDEC_ID       0x9F   /* Read JEDEC ID */
#define FLASH_CMD_READ_UNIQUE_ID      0x4B   /* Read Unique ID (8 bytes) */
#define FLASH_CMD_POWER_DOWN          0xB9   /* Deep Power Down */
#define FLASH_CMD_RELEASE_POWER_DOWN  0xAB   /* Release Power Down / Read ID */

/* Status Register 1 bits */
#define FLASH_SR1_WIP                 0x01   /* Write In Progress (BUSY) */
#define FLASH_SR1_WEL                 0x02   /* Write Enable Latch */
#define FLASH_SR1_BP0                 0x04   /* Block Protect 0 */
#define FLASH_SR1_BP1                 0x08   /* Block Protect 1 */
#define FLASH_SR1_BP2                 0x10   /* Block Protect 2 */
#define FLASH_SR1_TB                  0x20   /* Top/Bottom Protect */
#define FLASH_SR1_SEC                 0x40   /* Sector/Block Protect */
#define FLASH_SR1_SRP0                0x80   /* Status Register Protect 0 */

/* Status Register 2 bits */
#define FLASH_SR2_SRP1                0x01   /* Status Register Protect 1 */
#define FLASH_SR2_QE                  0x02   /* Quad Enable */
#define FLASH_SR2_LB1                 0x08   /* Security Register Lock 1 */
#define FLASH_SR2_LB2                 0x10   /* Security Register Lock 2 */
#define FLASH_SR2_LB3                 0x20   /* Security Register Lock 3 */
#define FLASH_SR2_CMP                 0x40   /* Complement Protect */
#define FLASH_SR2_SUS                 0x80   /* Suspend Status */

/* GD25Q32CS1G JEDEC ID */
#define GD25Q32_MANUFACTURER_ID       0xC8   /* GigaDevice */
#define GD25Q32_MEMORY_TYPE           0x40   /* SPI NOR Flash, 3.3V */
#define GD25Q32_CAPACITY_ID           0x16   /* 32Mbit = 4MB */

/* GD25Q32CS1G capacity */
#define GD25Q32_FLASH_SIZE            (4UL * 1024UL * 1024UL)   /* 4MB */
#define GD25Q32_PAGE_SIZE             256UL                      /* 256 bytes per page */
#define GD25Q32_SECTOR_SIZE           4096UL                     /* 4KB per sector */
#define GD25Q32_BLOCK_SIZE_32K        (32UL * 1024UL)            /* 32KB block */
#define GD25Q32_BLOCK_SIZE_64K        (64UL * 1024UL)            /* 64KB block */
#define GD25Q32_SIZE_MAX              (896UL * 1024UL )        /* 896KB max address */ 

/* GD25Q32CS1G address */
#define GD25Q32_INFO_ADDR               0X0F0000   
#define GD25Q32_UPDATE_ADDR             0X100000  

/* function declarations */
uint32_t gd25q32_read_jedec_id(void);
void gd25q32_write_enable(void);
void gd25q32_wait_busy(void);
void gd25q32_erase_sector(uint32_t addr);
void gd25q32_erase_block_64k(uint32_t addr);
void gd25q32_erase_chip(void);
void gd25q32_page_program(uint32_t addr, uint8_t *data, uint16_t len);
void gd25q32_read_data(uint32_t addr, uint8_t *data, uint16_t len);


void jump_to_app(uint32_t app_addr);

#endif


