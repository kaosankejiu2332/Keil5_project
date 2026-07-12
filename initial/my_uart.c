#include "my_uart.h"
#include "gd32f4xx.h"
#include "systick.h"
#include <stdio.h>




/*
 * 调试串口初始化
 * 参数: DEBUG_UART_BAUDRATE - 8 - N - 1
 */
void uart0_init(void)
{
    rcu_periph_clock_enable(DEBUG_UART_GPIO_RCU);
    rcu_periph_clock_enable(DEBUG_UART_RCU);

    /* TX / RX 复用 */
    gpio_af_set(DEBUG_UART_GPIO_PORT, DEBUG_UART_AF,
                DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);
    gpio_mode_set(DEBUG_UART_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);
    gpio_output_options_set(DEBUG_UART_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);

    /* 参数配置 */
    usart_baudrate_set(DEBUG_UART, DEBUG_UART_BAUDRATE);
    usart_parity_config(DEBUG_UART, USART_PM_NONE);
    usart_word_length_set(DEBUG_UART, USART_WL_8BIT);
    usart_stop_bit_set(DEBUG_UART, USART_STB_1BIT);

    /* 使能收发 */
    usart_transmit_config(DEBUG_UART, USART_TRANSMIT_ENABLE);
    usart_receive_config(DEBUG_UART, USART_RECEIVE_ENABLE);

    usart_enable(DEBUG_UART);
}

/*
 * USART0 DMA 接收初始化
 * 方向: 外设 → 存储器
 * 数据从 USART0 DATA 寄存器自动搬运到 buf
 * 参数: buf  - 接收缓冲区指针
 *        size - 缓冲区大小 (DMA 传输数量)
 */

uint32_t uart0_dma_buf_size;

void uart0_dma_rx_init(uint8_t *buf, uint32_t size)
{
    dma_single_data_parameter_struct dma_init_struct;

    uart0_dma_buf_size = size;      /* 记下缓冲区大小，ISR 中计算已收字节 */

    /* 使能 DMA 时钟 */
    rcu_periph_clock_enable(DEBUG_UART_DMA_RCU);

    /* 初始化 DMA 单数据模式结构体 */
    dma_single_data_para_struct_init(&dma_init_struct);

    dma_init_struct.direction   = DMA_PERIPH_TO_MEMORY;     /* 外设 → 存储器 */
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(DEBUG_UART); /* 外设地址: USART DATA 寄存器 */
    dma_init_struct.periph_inc  = DMA_PERIPH_INCREASE_DISABLE;    /* 外设地址不变 */
    dma_init_struct.memory0_addr = (uint32_t)buf;                 /* 存储器地址: 接收缓冲 */
    dma_init_struct.memory_inc  = DMA_MEMORY_INCREASE_ENABLE;     /* 存储器地址递增 */
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;  /* 8 位数据宽度 */
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;     /* 单次传输 */
    dma_init_struct.priority    = DMA_PRIORITY_HIGH;
    dma_init_struct.number      = size;                           /* 传输数量 */

    /* 应用配置 */
    dma_single_data_mode_init(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, &dma_init_struct);

    /* 选择外设: USART0_RX */
    dma_channel_subperipheral_select(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                                      DEBUG_UART_DMA_SUBPERI);

    /* 使能 USART0 DMA 接收 */
    usart_dma_receive_config(DEBUG_UART, USART_RECEIVE_DMA_ENABLE);

    /* 使能 DMA 通道 */
    dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);

    /* 最后开 IDLE 中断——此时 DMA 已就绪 */
    usart_interrupt_enable(DEBUG_UART, USART_INT_IDLE);
    nvic_irq_enable(USARTx_IRQn, 0, 1);
}

/* 发送一个字节 */
void uart0_send_byte(uint8_t byte)
{
    while (RESET == usart_flag_get(DEBUG_UART, USART_FLAG_TBE));
    usart_data_transmit(DEBUG_UART, byte);
}

/* 接收一个字节 */
uint8_t uart0_recv_byte(void)
{
    while (RESET == usart_flag_get(DEBUG_UART, USART_FLAG_RBNE));
    return (uint8_t)usart_data_receive(DEBUG_UART);
}

/* 发送任意字节 */
void uart0_send_bytes(uint8_t *data, uint16_t len)
{
    while (len--) {
        while (RESET == usart_flag_get(DEBUG_UART, USART_FLAG_TBE));
        usart_data_transmit(DEBUG_UART, *data++);
    }
}

/* 发送字符串 (以 '\0' 结尾) */
void uart0_send_string(char *str)
{
    while (*str) {
        while (RESET == usart_flag_get(DEBUG_UART, USART_FLAG_TBE));
        usart_data_transmit(DEBUG_UART, (uint8_t)*str++);
    }
}



/* ──────────────────────────────────────────────
 * LED 指示
 * ──────────────────────────────────────────────*/

void led_init(void)
{
    rcu_periph_clock_enable(LED_GPIO_RCU);
    gpio_mode_set(LED_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, LED_PIN);
    gpio_output_options_set(LED_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, LED_PIN);
    gpio_bit_set(LED_GPIO_PORT, LED_PIN);
}

void led_toggle(void)
{
    gpio_bit_reset(LED_GPIO_PORT, LED_PIN);
    delay_ms(500);
    gpio_bit_set(LED_GPIO_PORT, LED_PIN);
    delay_ms(500);
}

/* ──────────────────────────────────────────────
 * USART0 DMA + IDLE 中断
 * ──────────────────────────────────────────────*/

static volatile uint8_t  uart0_rx_done;      /* 接收完成标志 */
static volatile uint32_t uart0_rx_len;       /* 实际接收字节数 */


/* 获取接收完成标志 (供外部查询) */
uint8_t uart0_is_rx_done(void)
{
    return uart0_rx_done;
}

/* 获取实际接收长度 */
uint32_t uart0_get_rx_len(void)
{
    return uart0_rx_len;
}

/* 清除接收标志，准备下次接收 */
void uart0_rx_restart(void)
{
    uart0_rx_done = 0;
    uart0_rx_len  = 0;

    /* 重新使能 DMA，开始下次接收 */
    // dma_channel_disable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    // dma_transfer_number_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, uart0_dma_buf_size);
    // dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
}

uint8_t URX_BUFF[UART0_RX_BUF_SIZE]={0};
URX_CB urx;

/* overflow 保护: 丢弃缓冲区 */
static uint8_t uart0_drop_buf[UART0_RX_MAX];


//IN/OUT/END指针初始化
void ptr_init(void)
{
    urx.IN=urx.URX_ptr;
    urx.OUT=urx.URX_ptr;
    urx.END=&urx.URX_ptr[URX_ptr_SIZE-1];
    urx.IN->start=&URX_BUFF[0];
    urx.OUT->start=&URX_BUFF[0];
    urx.rx_count=0;

    /* 初始化 overflow 相关字段 */
    urx.overflow = 0;
    urx.overflow_count = 0;
    urx.dma_on_drop_buf = 0;
}




/* USART0 中断服务函数 */
void USART0_IRQHandler(void)
{
    /* 空闲中断: 串口 RX 线上无数据持续一个帧周期 → 一帧接收结束 */
    if (RESET != usart_interrupt_flag_get(DEBUG_UART, USART_INT_FLAG_IDLE))
    {
        uint32_t remain;
        URX_Buffptr *next_in;
        uint8_t *next_dma_start;
        uint32_t next_rx_count;

        /* ① 先停 DMA, 清完成标志, 读剩余字节数 */
        dma_channel_disable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
        dma_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, DMA_FLAG_FTF);
        dma_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, DMA_FLAG_HTF);
        remain = dma_transfer_number_get(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);

        /* ② 清除空闲中断标志 */
        usart_flag_get(USART0, USART_FLAG_IDLE);
        usart_data_receive(USART0);

        /* ③ 实际接收 = 总大小 - 剩余 */
        uart0_rx_len  = uart0_dma_buf_size - remain;

        /* 虚假 IDLE 过滤: 无数据 → 重开 DMA 直接返回，不污染指针 */
        if (uart0_rx_len == 0) {
            dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
            return;
        }

        /* ④ 如果 DMA 在 drop_buf 上，说明之前已经 overflow，只记录次数，不入队 */
        if (urx.dma_on_drop_buf) {
            urx.overflow = 1;
            urx.overflow_count++;

            /* 继续让 DMA 写到 drop_buf，保护正常缓冲区 */
            dma_memory_address_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                                      DMA_MEMORY_0, (uint32_t)uart0_drop_buf);
            dma_transfer_number_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, uart0_dma_buf_size);
            usart_dma_receive_config(DEBUG_UART, USART_RECEIVE_DMA_ENABLE);
            dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
            return;
        }

        /* ⑤ 正常情况：封口当前帧 */
        urx.rx_count += uart0_rx_len;
        uart0_rx_done = 1;
        urx.IN->end = &URX_BUFF[urx.rx_count - 1];

        /* ⑥ 计算下一个 IN 指针位置 */
        if (urx.IN == urx.END)
            next_in = urx.URX_ptr;
        else
            next_in = urx.IN + 1;

        /* ⑦ 判断两类 overflow */

        /* 情况 1：描述符环满（next_in 追上 OUT） */
        if (next_in == urx.OUT) {
            urx.overflow = 1;
            urx.overflow_count++;
            urx.dma_on_drop_buf = 1;

            /* 发布当前帧 */
            urx.IN = next_in;

            /* 下次 DMA 写到丢弃缓冲区 */
            next_dma_start = uart0_drop_buf;
            goto restart_dma;
        }

        /* 情况 2：字节缓冲区需要回绕，但会覆盖未消费数据 */
        if (UART0_RX_BUF_SIZE - urx.rx_count < UART0_RX_MAX) {
            /* 需要回绕到开头 */
            next_dma_start = &URX_BUFF[0];
            next_rx_count = 0;

            /* 判断回绕会不会覆盖未消费数据 */
            /* 如果 OUT 还在前半段（urx.OUT->start < next_dma_start + UART0_RX_MAX），就会冲突 */
            if (urx.OUT != urx.IN) {  /* 队列非空 */
                /* 简化判断：如果回绕后下一帧可能写到 [0, UART0_RX_MAX) 范围，
                   而 OUT 槽的 start 也在这个范围内，就可能覆盖 */
                if (urx.OUT->start >= &URX_BUFF[0] &&
                    urx.OUT->start < &URX_BUFF[UART0_RX_MAX]) {
                    /* 会覆盖未消费数据 → overflow */
                    urx.overflow = 1;
                    urx.overflow_count++;
                    urx.dma_on_drop_buf = 1;

                    /* 发布当前帧 */
                    urx.IN = next_in;

                    /* 下次 DMA 写到丢弃缓冲区 */
                    next_dma_start = uart0_drop_buf;
                    goto restart_dma;
                }
            }

            /* 回绕安全，可以继续 */
            urx.IN = next_in;
            urx.IN->start = next_dma_start;
            urx.rx_count = next_rx_count;
        } else {
            /* 不需要回绕，顺序前进 */
            next_dma_start = &URX_BUFF[urx.rx_count];
            urx.IN = next_in;
            urx.IN->start = next_dma_start;
        }

restart_dma:
        /* ⑧ 重启 DMA */
        dma_memory_address_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                                  DMA_MEMORY_0, (uint32_t)next_dma_start);
        dma_transfer_number_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH, uart0_dma_buf_size);
        usart_dma_receive_config(DEBUG_UART, USART_RECEIVE_DMA_ENABLE);
        dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    }
}

/* overflow 检测接口实现 */
uint8_t uart0_rx_is_overflow(void)
{
    return urx.overflow;
}

void uart0_rx_clear_overflow(void)
{
    __disable_irq();
    urx.overflow = 0;
    urx.dma_on_drop_buf = 0;
    __enable_irq();
}
