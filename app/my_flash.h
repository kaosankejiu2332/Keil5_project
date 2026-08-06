#ifndef __MY_FLASH_H_
#define __MY_FLASH_H_

#include <stdint.h>

uint8_t flash_sector_erase(uint32_t firmware_size);
uint8_t flash_update_data(uint32_t firmware_size);

#endif
