#ifndef __MY_UART_H__
#define __MY_UART_H__

#include "gd32f4xx.h"
#include <stdint.h>

#define DEBUG_UART              USART0
#define DEBUG_UART_RCU          RCU_USART0
#define DEBUG_UART_GPIO_PORT    GPIOA
#define DEBUG_UART_GPIO_RCU     RCU_GPIOA
#define DEBUG_UART_AF           GPIO_AF_7
#define DEBUG_UART_TX_PIN       GPIO_PIN_9
#define DEBUG_UART_RX_PIN       GPIO_PIN_10
#define DEBUG_UART_BAUDRATE     115200U

/* GD32F427: USART0_RX = DMA1 channel 2, subperipheral 4. */
#define DEBUG_UART_DMA          DMA1
#define DEBUG_UART_DMA_RCU      RCU_DMA1
#define DEBUG_UART_DMA_CH       DMA_CH2
#define DEBUG_UART_DMA_SUBPERI  DMA_SUBPERI4
#define DEBUG_UART_DMA_IRQn     DMA1_Channel2_IRQn

/* About 711 ms of buffering at 115200 baud (8-N-1). */
#define UART0_DMA_RX_BUF_SIZE   8192U

#define UART0_RX_ERROR_NONE      0x00U
#define UART0_RX_ERROR_OVERFLOW  0x01U
#define UART0_RX_ERROR_DMA       0x02U
#define UART0_RX_ERROR_USART     0x04U

#define LED_GPIO_PORT           GPIOC
#define LED_GPIO_RCU            RCU_GPIOC
#define LED_PIN                 GPIO_PIN_8

void led_init(void);
void led_toggle(void);

void uart0_init(void);
void uart0_dma_rx_init(void);
void uart0_rx_flush(void);
uint32_t uart0_rx_available(void);
uint32_t uart0_rx_read(uint8_t *dst, uint32_t max_len);
uint8_t uart0_rx_is_overflow(void);
uint8_t uart0_rx_get_error(void);
void uart0_rx_clear_overflow(void);

void uart0_send_byte(uint8_t byte);
uint8_t uart0_recv_byte(void);
void uart0_send_bytes(const uint8_t *data, uint16_t len);
void uart0_send_string(const char *str);

#endif
