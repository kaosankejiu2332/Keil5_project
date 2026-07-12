/**
  ******************************************************************************
  * @file    usart1.c
  * @brief   USART1初始化配置源文件
  * @target  GD32F427
  * @note    USART1使用PA9(TX)和PA10(RX)引脚
  ******************************************************************************
  */

#include "usart1.h"

/* 接收缓冲区 */
static uint8_t usart1_rx_buffer[USART1_RX_BUFFER_SIZE];
static volatile uint16_t usart1_rx_index = 0;

/**
  * @brief  USART1初始化函数
  * @param  None
  * @retval None
  * @note   波特率: 115200, 数据位: 8, 停止位: 1, 校验位: 无
  */
void usart1_init(void)
{
    /* 使能GPIOA和USART1时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART1);
    
    /* 配置USART1引脚 */
    /* PA9 - USART1_TX (复用推挽输出) */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9);
    
    /* PA10 - USART1_RX (复用输入) */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
    gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_10);
    
    /* USART1去初始化 */
    usart_deinit(USART1);
    
    /* 配置USART1参数 */
    usart_baudrate_set(USART1, USART1_BAUDRATE);              // 波特率: 115200
    usart_word_length_set(USART1, USART_WL_8BIT);             // 数据位: 8位
    usart_stop_bit_set(USART1, USART_STB_1BIT);               // 停止位: 1位
    usart_parity_config(USART1, USART_PM_NONE);               // 校验位: 无
    usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE); // RTS禁用
    usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE); // CTS禁用
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);       // 使能接收
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);     // 使能发送
    
    /* 使能USART1 */
    usart_enable(USART1);
    
    /* 使能USART1接收中断 */
    usart_interrupt_enable(USART1, USART_INT_RBNE);
    
    /* 配置USART1中断优先级 */
    nvic_irq_enable(USART1_IRQn, 2, 0);
}

/**
  * @brief  USART1发送一个字节
  * @param  data: 要发送的数据
  * @retval None
  */
void usart1_send_byte(uint8_t data)
{
    usart_data_transmit(USART1, data);
    while(RESET == usart_flag_get(USART1, USART_FLAG_TBE));
}

/**
  * @brief  USART1发送字符串
  * @param  str: 要发送的字符串指针
  * @retval None
  */
void usart1_send_string(const char *str)
{
    while(*str)
    {
        usart1_send_byte(*str++);
    }
}

/**
  * @brief  USART1接收一个字节(阻塞方式)
  * @param  None
  * @retval 接收到的数据
  */
uint8_t usart1_receive_byte(void)
{
    while(RESET == usart_flag_get(USART1, USART_FLAG_RBNE));
    return usart_data_receive(USART1);
}

/**
  * @brief  重定向printf到USART1
  * @param  ch: 字符
  * @param  f: 文件指针
  * @retval 发送的字符
  */
int fputc(int ch, FILE *f)
{
    usart1_send_byte((uint8_t)ch);
    return ch;
}

/**
  * @brief  USART1中断服务函数
  * @param  None
  * @retval None
  * @note   在中断中处理接收数据
  */
void USART1_IRQHandler(void)
{
    if(RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE))
    {
        /* 读取接收到的数据 */
        uint8_t data = usart_data_receive(USART1);
        
        /* 存储到接收缓冲区 */
        if(usart1_rx_index < USART1_RX_BUFFER_SIZE)
        {
            usart1_rx_buffer[usart1_rx_index++] = data;
        }
        else
        {
            /* 缓冲区满，重置索引 */
            usart1_rx_index = 0;
        }
    }
}
