#ifndef __MY_FLASH_H_
#define __MY_FLASH_H_

#include "stdint.h"





/*────GD25Q32存放的地址─────────────────────────────────*/

// #define FLASH_FW_STORE_ADDR          0x100000   /* 固件在 GD25Q32 存放起始 */
// #define FLASH_INFO_ADDR              0x0F0000   /* 升级信息存放地址 */
// #define BOOT_MAGIC                   0xAA55AA55 /* 有效固件标志 */


void flash_sector_erase(void);
void flash_update_data(uint32_t firmware_size);



#endif



