#ifndef __USER_GPIO_H__
#define __USER_GPIO_H__

#include <stdint.h>

/*──────────────────────────────────────────────
 * 外设引脚映射 (修改此处即可移植)
 *──────────────────────────────────────────────*/

/* ── 调试串口 ──────────────────────────────── */
#define DEBUG_UART              USART0
#define USARTx_IRQn             USART0_IRQn
#define DEBUG_UART_RCU          RCU_USART0
#define DEBUG_UART_GPIO_PORT    GPIOA
#define DEBUG_UART_GPIO_RCU     RCU_GPIOA
#define DEBUG_UART_AF           GPIO_AF_7
#define DEBUG_UART_TX_PIN       GPIO_PIN_9
#define DEBUG_UART_RX_PIN       GPIO_PIN_10
#define DEBUG_UART_BAUDRATE     115200U

/* USART0 DMA 接收 — GD32F427: USART0_RX = DMA1 CH2 SUBPERI4 */
#define DEBUG_UART_DMA          DMA1
#define DEBUG_UART_DMA_RCU      RCU_DMA1
#define DEBUG_UART_DMA_CH       DMA_CH2
#define DEBUG_UART_DMA_SUBPERI  DMA_SUBPERI4



/* ── 状态 LED ──────────────────────────────── */
#define LED_GPIO_PORT           GPIOC
#define LED_GPIO_RCU            RCU_GPIOC
#define LED_PIN                 GPIO_PIN_8



/*────串口空闲中断缓存数组─────────────────────────────────*/
#define UART0_RX_BUF_SIZE         4096
#define UART0_RX_MAX              200//剩余最大空间
#define URX_ptr_SIZE              10//记录10次收发

/*────标记缓存数组位置指针─────────────────────────────────*/
typedef struct
{
    uint8_t * volatile start;
    uint8_t * volatile end;
} URX_Buffptr;

typedef struct
{
    volatile uint32_t rx_count;//累计接收了多少
    URX_Buffptr URX_ptr[URX_ptr_SIZE];//第几次存取
    URX_Buffptr * volatile IN;
    URX_Buffptr * volatile OUT;
    URX_Buffptr * volatile END;

    /* overflow 检测相关 */
    volatile uint8_t  overflow;          // 溢出标志: 1=发生过溢出
    volatile uint32_t overflow_count;    // 累计溢出次数
    volatile uint8_t  dma_on_drop_buf;   // DMA 是否在丢弃缓冲区上工作
} URX_CB;


extern uint8_t URX_BUFF[UART0_RX_BUF_SIZE];
extern URX_CB urx;
extern uint32_t uart0_dma_buf_size;
/*──────────────────────────────────────────────
 * 函数声明
 *──────────────────────────────────────────────*/
void led_init(void);
void led_toggle(void);

/* 调试串口 */
void uart0_init(void);
void     uart0_dma_rx_init(uint8_t *buf, uint32_t size);    /* DMA 接收初始化 */
uint8_t  uart0_is_rx_done(void);                             /* 接收完成? 1=完成 */
uint32_t uart0_get_rx_len(void);                             /* 实际接收字节数 */
void     uart0_rx_restart(void);                             /* 清除标志, 重新接收 */
void uart0_send_byte(uint8_t byte);
uint8_t uart0_recv_byte(void);
void uart0_send_bytes(uint8_t *data, uint16_t len);
void uart0_send_string(char *str);


/* 指针 */
void ptr_init(void);

/* overflow 检测接口 */
uint8_t uart0_rx_is_overflow(void);       /* 查询是否溢出 */
void    uart0_rx_clear_overflow(void);    /* 清除溢出标志 */

#endif
