#ifndef __UART_IAP_H_
#define __UART_IAP_H_

#include "gd32f4xx.h"

#define IAP_FLAG_IDLE        0x00   // 空闲，等 "update"
#define IAP_FLAG_HANDSHAKE   0x01   // 握手阶段，发 'C' 等 SOH
#define IAP_FLAG_RECEIVING   0x02   // 收包阶段，解析 Xmodem 133 字节
#define IAP_FLAG_DONE        0x04   // 传输完成

typedef struct
{
    uint8_t delay_flag;  //延时标志位
    uint8_t delay_count;
    uint8_t sta_flag;    //状态机标志位
    uint16_t rx_packnum; //接收包数
} IAP_status;

void uart_iap_init(void);
void uart_iap_feed(uint8_t *data, uint16_t len);
void uart_iap_poll(void);
uint16_t Xmode_CRC16(uint8_t *data, uint16_t len);

#endif
