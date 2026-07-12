/**
  ******************************************************************************
  * @file    usart1.h
  * @brief   USART1初始化配置头文件
  * @target  GD32F427
  ******************************************************************************
  */

#ifndef __USART1_H
#define __USART1_H

#include "gd32f4xx.h"
#include <stdio.h>

/* USART1波特率定义 */
#define USART1_BAUDRATE         115200U

/* 接收缓冲区大小 */
#define USART1_RX_BUFFER_SIZE   256

/* 函数声明 */
void usart1_init(void);
void usart1_send_byte(uint8_t data);
void usart1_send_string(const char *str);
uint8_t usart1_receive_byte(void);
int fputc(int ch, FILE *f);

#endif /* __USART1_H */
