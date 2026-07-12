#include "my_spi.h"
/* ──────────────────────────────────────────────
 * SPI Flash 驱动
 * ──────────────────────────────────────────────*/

/* NSS 控制 (静态函数，仅本文件内部使用) */
// static void spi1_nss_low(void)
// {
//     gpio_bit_reset(FLASH_NSS_GPIO_PORT, FLASH_NSS_PIN);
// }

static void spi1_nss_high(void)
{
    gpio_bit_set(FLASH_NSS_GPIO_PORT, FLASH_NSS_PIN);
}

/* 收发一个字节 */
uint8_t spi1_read_write_byte(uint8_t byte)
{
    while (RESET == spi_i2s_flag_get(FLASH_SPI, SPI_FLAG_TBE));
    spi_i2s_data_transmit(FLASH_SPI, byte);
    while (RESET == spi_i2s_flag_get(FLASH_SPI, SPI_FLAG_RBNE));
    return spi_i2s_data_receive(FLASH_SPI);
}

/* SPI 初始化 */
void spi1_init(void)
{
    spi_parameter_struct spi_init_struct;

    /* 使能 GPIO 与 SPI 时钟 */
    rcu_periph_clock_enable(FLASH_NSS_GPIO_RCU);
    rcu_periph_clock_enable(FLASH_SPI_GPIO_RCU);
    rcu_periph_clock_enable(FLASH_SPI_RCU);

    /* NSS: 推挽输出，软件控制片选 */
    gpio_mode_set(FLASH_NSS_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP,
                  FLASH_NSS_PIN);
    gpio_output_options_set(FLASH_NSS_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            FLASH_NSS_PIN);
    spi1_nss_high();

    /* SCK / MISO / MOSI 复用 */
    gpio_af_set(FLASH_SPI_GPIO_PORT, FLASH_SPI_AF,
                FLASH_SPI_SCK_PIN | FLASH_SPI_MISO_PIN | FLASH_SPI_MOSI_PIN);
    gpio_mode_set(FLASH_SPI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  FLASH_SPI_SCK_PIN | FLASH_SPI_MISO_PIN | FLASH_SPI_MOSI_PIN);
    gpio_output_options_set(FLASH_SPI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            FLASH_SPI_SCK_PIN | FLASH_SPI_MISO_PIN | FLASH_SPI_MOSI_PIN);

    /* 参数配置 */
    spi_struct_para_init(&spi_init_struct);
    spi_init_struct.device_mode           = SPI_MASTER;
    spi_init_struct.trans_mode            = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.frame_size            = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase  = SPI_CK_PL_LOW_PH_1EDGE;
    spi_init_struct.nss                   = SPI_NSS_SOFT;
    spi_init_struct.prescale              = SPI_PSC_4;
    spi_init_struct.endian                = SPI_ENDIAN_MSB;

    spi_init(FLASH_SPI, &spi_init_struct);
    spi_enable(FLASH_SPI);
}


