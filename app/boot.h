#ifndef __BOOT_H_
#define __BOOT_H_

#include "gd32f4xx.h"

#define APP_START_ADDR  0x08020000UL
#define APP_MAX_SIZE    0x000E0000UL
#define APP_END_ADDR    (APP_START_ADDR + APP_MAX_SIZE)
#define BOOT_UPDATE_MAGIC 0xAABBCCDDUL

typedef void (*app_func_t)(void);

uint8_t app_vector_is_valid(uint32_t app_addr);
void boot_clear(void);
void jump_to_app(uint32_t app_addr);
void boot_branches(void);

#endif
