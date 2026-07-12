#ifndef __BOOT_H_
#define __BOOT_H_


#include "gd32f4xx.h"

#define APP_START_ADDR              0x08020000 /* MCU Flash APP 起始 */

typedef void (*app_func_t)(void);//定义一个指向无返回，无参数函数的指针类型

void boot_clear(void);
void jump_to_app(uint32_t app_addr);
void boot_branches(void);

#endif



