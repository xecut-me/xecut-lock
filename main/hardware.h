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

// Functions.
void hardware_setup(void);
