#ifndef __UART_IAP_H_
#define __UART_IAP_H_

#include "gd32f4xx.h"

#define IAP_FLAG_IDLE        0x00U
#define IAP_FLAG_PREPARING   0x01U
#define IAP_FLAG_RECEIVING   0x02U
#define IAP_FLAG_VERIFYING   0x03U
#define IAP_FLAG_DONE        0x04U
#define IAP_FLAG_ERROR       0x05U

#define IAP_APP_MAX_SIZE        0x000E0000UL

typedef enum {
    IAP_ERROR_NONE = 0,
    IAP_ERROR_COMMAND,
    IAP_ERROR_SIZE,
    IAP_ERROR_OVERFLOW,
    IAP_ERROR_UART,
    IAP_ERROR_FLASH
} iap_error_t;

typedef struct {
    volatile uint8_t sta_flag;
    volatile uint8_t error;
    uint32_t expected_size;
    uint32_t received_size;
    uint32_t written_size;
} IAP_status;

extern IAP_status uart_iap;

void uart_iap_init(void);
void uart_iap_feed(const uint8_t *data, uint32_t len);
void uart_iap_poll(void);
void uart_iap_abort(iap_error_t error);

#endif
