#include "hardware.h"

#include <driver/spi_common.h>
#include <driver/gpio.h>

static void init_eth_spi(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_SPI_MISO_GPIO,
        .mosi_io_num = ETH_SPI_MOSI_GPIO,
        .sclk_io_num = ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

void hardware_setup(void) {
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    init_eth_spi();
}
