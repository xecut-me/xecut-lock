#include "hardware.h"

#include <esp_err.h>
#include <driver/spi_common.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/queue.h>

#include "config.h"

static void setup_lock_gpio(void) {
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << LOCK_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));

    gpio_set_level(LOCK_GPIO, !LOCK_OPENED_LOGIC_LEVEL);
}

#ifndef USE_WIFI
static void setup_eth_spi(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_SPI_MISO_GPIO,
        .mosi_io_num = ETH_SPI_MOSI_GPIO,
        .sclk_io_num = ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
}
#endif

static void setup_keypad_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = KEYPAD_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(
        KEYPAD_UART_NUM,
        KEYPAD_UART_BUFFER_SIZE * 2,
        0,
        0,
        NULL,
        0
    ));

    ESP_ERROR_CHECK(uart_param_config(
        KEYPAD_UART_NUM,
        &uart_config
    ));

    ESP_ERROR_CHECK(uart_set_pin(
        KEYPAD_UART_NUM,
        KEYPAD_UART_TX,
        KEYPAD_UART_RX,
        /* RTS */ UART_PIN_NO_CHANGE,
        /* CTS */ UART_PIN_NO_CHANGE
    ));
}

void hardware_setup(void) {
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    setup_lock_gpio();
#ifndef USE_WIFI
    setup_eth_spi();
#endif
    setup_keypad_uart();
}
