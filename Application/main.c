/*!
    \file    main.c
    \brief   led spark with systick

    \version 2026-02-05, V3.3.3, firmware for GD32F4xx
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx.h"
#include "systick.h"
#include "main.h"
#include "gd32f450i_eval.h"
#include "my_uart.h"
#include "my_spi.h"
#include "uart_iap.h"
#include <stdio.h>

// extern URX_CB urx;
// extern uint8_t URX_BUFF[UART0_RX_BUF_SIZE];
extern volatile uint32_t isr_dbg_remain, isr_dbg_len;
extern volatile int32_t  isr_dbg_in_idx, isr_dbg_out_idx;
extern  IAP_status uart_iap;

/*!
    \brief    main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    uint16_t receive_len;
    uint8_t *ptr;
    uint32_t now=0,first_tick=0;

    systick_config_ms();
    ptr_init();                                         /* ① 先初始化环形缓冲指针 */
    uart0_init();                                       /* ② 再开 USART  */
    uart0_dma_rx_init(URX_BUFF, UART0_RX_BUF_SIZE);     /* ③ 最后开 DMA + IDLE 中断*/
    led_init();
    spi1_init();
    uart_iap_init();

    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    while(1) {
        if(urx.OUT != urx.IN) {
            receive_len = urx.OUT->end - urx.OUT->start + 1;
            ptr = urx.OUT->start;

            uart_iap_feed(ptr, receive_len);

            if(urx.OUT == urx.END) {
                urx.OUT = urx.URX_ptr;
            } else {
                urx.OUT++;
            }
        }

        uart_iap_poll();
			 if(uart_iap.sta_flag == IAP_FLAG_IDLE) {
			
			 now=get_tick();
             if(now-first_tick>1000) {
                 first_tick=now;
                 led_toggle();
				printf("how it can\r\n");
             }
			 }
    }
}


