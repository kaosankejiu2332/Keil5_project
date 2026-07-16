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

typedef struct
{
    uint8_t code;          // 最后一次失败原因
    uint8_t state;         // 失败时状态机状态
    uint8_t expected_blk;  // 失败时期望包号
    uint8_t rx_blk;        // 失败时收到的包号/字节
    uint16_t frame_len;    // 当前 uart_iap_feed 收到的帧长
    uint16_t stream_len;   // 当前已组包字节数
    uint8_t queue_count;   // payload 队列深度
    uint16_t page_fill;    // 页缓存已填充字节数
} IAP_DebugInfo;

#define IAP_ERR_NONE                0x00
#define IAP_ERR_HDR_SUM             0x01
#define IAP_ERR_HANDSHAKE_BLK       0x02
#define IAP_ERR_HANDSHAKE_CRC       0x03
#define IAP_ERR_HANDSHAKE_QUEUE     0x04
#define IAP_ERR_RECEIVE_CRC         0x05
#define IAP_ERR_RECEIVE_QUEUE       0x06
#define IAP_ERR_RECEIVE_BLK         0x07
#define IAP_ERR_STREAM_NOISE        0x08
#define IAP_ERR_HANDSHAKE_TIMEOUT   0x09
#define IAP_ERR_RECEIVE_TIMEOUT     0x0A
#define IAP_ERR_STREAM_TIMEOUT      0x0B
#define IAP_ERR_STREAM_RESYNC       0x0C

#define IAP_STREAM_TIMEOUT_MS       1000U
#define IAP_HANDSHAKE_TIMEOUT_MS    15000U
#define IAP_RECEIVE_TIMEOUT_MS      3000U

extern volatile IAP_DebugInfo uart_iap_debug;

void uart_iap_init(void);
void uart_iap_feed(uint8_t *data, uint16_t len);
void uart_iap_poll(void);
uint16_t Xmode_CRC16(uint8_t *data, uint16_t len);
const char *uart_iap_debug_error_name(uint8_t code);

#endif
