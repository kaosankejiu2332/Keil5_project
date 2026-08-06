#include "my_uart.h"
#include "systick.h"
#include <string.h>

static uint8_t uart0_dma_rx_buf[UART0_DMA_RX_BUF_SIZE];
static volatile uint32_t uart0_dma_wrap_count;
static volatile uint8_t uart0_rx_error;
static uint32_t uart0_rx_consumed_total;

static uint32_t uart0_rx_produced_total(void)
{
    uint32_t wraps;
    uint32_t remain;
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();
    wraps = uart0_dma_wrap_count;
    remain = dma_transfer_number_get(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);

    /* Account for a completed wrap whose IRQ has not run yet. */
    if(SET == dma_interrupt_flag_get(DEBUG_UART_DMA,
                                     DEBUG_UART_DMA_CH,
                                     DMA_INT_FLAG_FTF)) {
        wraps++;
        remain = dma_transfer_number_get(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    }

    if(primask == 0U) {
        __enable_irq();
    }

    return (wraps * UART0_DMA_RX_BUF_SIZE)
           + (UART0_DMA_RX_BUF_SIZE - remain);
}

void uart0_init(void)
{
    rcu_periph_clock_enable(DEBUG_UART_GPIO_RCU);
    rcu_periph_clock_enable(DEBUG_UART_RCU);

    gpio_af_set(DEBUG_UART_GPIO_PORT, DEBUG_UART_AF,
                DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);
    gpio_mode_set(DEBUG_UART_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);
    gpio_output_options_set(DEBUG_UART_GPIO_PORT, GPIO_OTYPE_PP,
                            GPIO_OSPEED_50MHZ,
                            DEBUG_UART_TX_PIN | DEBUG_UART_RX_PIN);

    usart_deinit(DEBUG_UART);
    usart_baudrate_set(DEBUG_UART, DEBUG_UART_BAUDRATE);
    usart_parity_config(DEBUG_UART, USART_PM_NONE);
    usart_word_length_set(DEBUG_UART, USART_WL_8BIT);
    usart_stop_bit_set(DEBUG_UART, USART_STB_1BIT);
    usart_transmit_config(DEBUG_UART, USART_TRANSMIT_ENABLE);
    usart_receive_config(DEBUG_UART, USART_RECEIVE_ENABLE);
    usart_enable(DEBUG_UART);
}

void uart0_dma_rx_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(DEBUG_UART_DMA_RCU);
    dma_channel_disable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    dma_deinit(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    dma_single_data_para_struct_init(&dma_init_struct);

    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(DEBUG_UART);
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory0_addr = (uint32_t)uart0_dma_rx_buf;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.circular_mode = DMA_CIRCULAR_MODE_ENABLE;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init_struct.number = UART0_DMA_RX_BUF_SIZE;

    dma_single_data_mode_init(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                              &dma_init_struct);
    dma_channel_subperipheral_select(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                                     DEBUG_UART_DMA_SUBPERI);

    uart0_dma_wrap_count = 0U;
    uart0_rx_consumed_total = 0U;
    uart0_rx_error = UART0_RX_ERROR_NONE;

    dma_interrupt_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                             DMA_INT_FLAG_FTF);
    dma_interrupt_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                             DMA_INT_FLAG_TAE);
    dma_interrupt_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                         DMA_INT_FTF | DMA_INT_TAE);
    nvic_irq_enable(DEBUG_UART_DMA_IRQn, 0U, 1U);

    usart_flag_clear(DEBUG_UART, USART_FLAG_ORERR);
    usart_flag_clear(DEBUG_UART, USART_FLAG_NERR);
    usart_flag_clear(DEBUG_UART, USART_FLAG_FERR);
    usart_interrupt_enable(DEBUG_UART, USART_INT_ERR);
    nvic_irq_enable(USART0_IRQn, 0U, 2U);

    usart_dma_receive_config(DEBUG_UART, USART_RECEIVE_DMA_ENABLE);
    dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
}

void uart0_rx_flush(void)
{
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();
    dma_channel_disable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    dma_interrupt_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                             DMA_INT_FLAG_FTF);
    dma_interrupt_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                             DMA_INT_FLAG_TAE);
    dma_memory_address_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                              DMA_MEMORY_0, (uint32_t)uart0_dma_rx_buf);
    dma_transfer_number_config(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                               UART0_DMA_RX_BUF_SIZE);
    uart0_dma_wrap_count = 0U;
    uart0_rx_consumed_total = 0U;
    uart0_rx_error = UART0_RX_ERROR_NONE;
    usart_flag_clear(DEBUG_UART, USART_FLAG_ORERR);
    usart_flag_clear(DEBUG_UART, USART_FLAG_NERR);
    usart_flag_clear(DEBUG_UART, USART_FLAG_FERR);
    dma_channel_enable(DEBUG_UART_DMA, DEBUG_UART_DMA_CH);
    if(primask == 0U) {
        __enable_irq();
    }
}

uint32_t uart0_rx_available(void)
{
    uint32_t produced;
    uint32_t available;

    if(uart0_rx_error != UART0_RX_ERROR_NONE) {
        return 0U;
    }

    produced = uart0_rx_produced_total();
    available = produced - uart0_rx_consumed_total;
    if(available > UART0_DMA_RX_BUF_SIZE) {
        uart0_rx_error |= UART0_RX_ERROR_OVERFLOW;
        return 0U;
    }
    return available;
}

uint32_t uart0_rx_read(uint8_t *dst, uint32_t max_len)
{
    uint32_t available;
    uint32_t read_pos;
    uint32_t chunk;

    if((dst == 0) || (max_len == 0U)) {
        return 0U;
    }

    available = uart0_rx_available();
    if(available == 0U) {
        return 0U;
    }

    chunk = (available < max_len) ? available : max_len;
    read_pos = uart0_rx_consumed_total % UART0_DMA_RX_BUF_SIZE;
    if(chunk > (UART0_DMA_RX_BUF_SIZE - read_pos)) {
        chunk = UART0_DMA_RX_BUF_SIZE - read_pos;
    }

    memcpy(dst, &uart0_dma_rx_buf[read_pos], chunk);
    uart0_rx_consumed_total += chunk;
    return chunk;
}

uint8_t uart0_rx_is_overflow(void)
{
    (void)uart0_rx_available();
    return (uint8_t)((uart0_rx_error & UART0_RX_ERROR_OVERFLOW) != 0U);
}

uint8_t uart0_rx_get_error(void)
{
    (void)uart0_rx_available();
    return uart0_rx_error;
}

void uart0_rx_clear_overflow(void)
{
    uart0_rx_flush();
}

void DMA1_Channel2_IRQHandler(void)
{
    if(SET == dma_interrupt_flag_get(DEBUG_UART_DMA,
                                     DEBUG_UART_DMA_CH,
                                     DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                                 DMA_INT_FLAG_FTF);
        uart0_dma_wrap_count++;
    }

    if(SET == dma_interrupt_flag_get(DEBUG_UART_DMA,
                                     DEBUG_UART_DMA_CH,
                                     DMA_INT_FLAG_TAE)) {
        dma_interrupt_flag_clear(DEBUG_UART_DMA, DEBUG_UART_DMA_CH,
                                 DMA_INT_FLAG_TAE);
        uart0_rx_error |= UART0_RX_ERROR_DMA;
    }
}

void USART0_IRQHandler(void)
{
    uint8_t has_error = 0U;

    if(SET == usart_interrupt_flag_get(DEBUG_UART,
                                       USART_INT_FLAG_ERR_ORERR)) {
        usart_flag_clear(DEBUG_UART, USART_FLAG_ORERR);
        has_error = 1U;
    }
    if(SET == usart_interrupt_flag_get(DEBUG_UART,
                                       USART_INT_FLAG_ERR_NERR)) {
        usart_flag_clear(DEBUG_UART, USART_FLAG_NERR);
        has_error = 1U;
    }
    if(SET == usart_interrupt_flag_get(DEBUG_UART,
                                       USART_INT_FLAG_ERR_FERR)) {
        usart_flag_clear(DEBUG_UART, USART_FLAG_FERR);
        has_error = 1U;
    }

    if(has_error != 0U) {
        uart0_rx_error |= UART0_RX_ERROR_USART;
    }
}

void uart0_send_byte(uint8_t byte)
{
    while(RESET == usart_flag_get(DEBUG_UART, USART_FLAG_TBE)) {
    }
    usart_data_transmit(DEBUG_UART, byte);
}

uint8_t uart0_recv_byte(void)
{
    while(RESET == usart_flag_get(DEBUG_UART, USART_FLAG_RBNE)) {
    }
    return (uint8_t)usart_data_receive(DEBUG_UART);
}

void uart0_send_bytes(const uint8_t *data, uint16_t len)
{
    while(len-- > 0U) {
        uart0_send_byte(*data++);
    }
    while(RESET == usart_flag_get(DEBUG_UART, USART_FLAG_TC)) {
    }
}

void uart0_send_string(const char *str)
{
    while(*str != '\0') {
        uart0_send_byte((uint8_t)*str++);
    }
    while(RESET == usart_flag_get(DEBUG_UART, USART_FLAG_TC)) {
    }
}

void led_init(void)
{
    rcu_periph_clock_enable(LED_GPIO_RCU);
    gpio_mode_set(LED_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, LED_PIN);
    gpio_output_options_set(LED_GPIO_PORT, GPIO_OTYPE_PP,
                            GPIO_OSPEED_2MHZ, LED_PIN);
    gpio_bit_set(LED_GPIO_PORT, LED_PIN);
}

void led_toggle(void)
{
    static uint8_t led_on;

    led_on ^= 1U;
    if(led_on != 0U) {
        gpio_bit_reset(LED_GPIO_PORT, LED_PIN);
    } else {
        gpio_bit_set(LED_GPIO_PORT, LED_PIN);
    }
}

