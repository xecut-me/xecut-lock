#pragma once

// SPI for Ethernet.
#define ETH_SPI_HOST       1
#define ETH_SPI_CLOCK_MHZ  12

#define ETH_SPI_MOSI_GPIO  11
#define ETH_SPI_MISO_GPIO  12
#define ETH_SPI_SCLK_GPIO  13
#define ETH_SPI_CS_GPIO    14
#define ETH_SPI_INT_GPIO   10
#define ETH_SPI_RST_GPIO   9

// UART for Keypad
#define KEYPAD_UART_NUM          UART_NUM_2
#define KEYPAD_UART_BAUDRATE     9600
#define KEYPAD_UART_BUFFER_SIZE  512
#define KEYPAD_UART_TX           16
#define KEYPAD_UART_RX           18

// GPIO for Lock
#define LOCK_GPIO          GPIO_NUM_42
#define LOCK_OPEN_TIME_US  200000

// Functions.
void hardware_setup(void);
