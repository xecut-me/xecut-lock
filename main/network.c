#include "network.h"

#include <esp_log.h>
#include <esp_event.h>
#include <esp_eth.h>
#include <esp_eth_phy_w5500.h>
#include <esp_eth_mac_w5500.h>
#include <lwip/esp_netif_net_stack.h>
#include <driver/gpio.h>

#define TAG "eth"

#define SPI_HOST       1
#define SPI_CLOCK_MHZ  12

#define SPI_MOSI_GPIO  11
#define SPI_MISO_GPIO  12
#define SPI_SCLK_GPIO  13
#define SPI_CS_GPIO    14
#define SPI_INT_GPIO   10
#define SPI_RST_GPIO   9

const uint8_t MAC_ADDRESS[ETH_ADDR_LEN] = {
    0x0E, 0x00, 0xAF, 0x04, 0x38, 0x56
};

static void init_spi(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = SPI_MISO_GPIO,
        .mosi_io_num = SPI_MOSI_GPIO,
        .sclk_io_num = SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

static esp_eth_mac_t *eth_w5500_get_mac(void) {
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 24,
        .spics_io_num = SPI_CS_GPIO
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = SPI_INT_GPIO;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 4096;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);

    return mac;
}

static esp_eth_phy_t *eth_w5500_get_phy(void) {
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = SPI_RST_GPIO;

    return esp_eth_phy_new_w5500(&phy_config);
}

static void eth_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Link Up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGD(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGD(TAG, "Ethernet Stopped");
        break;
    default:
        ESP_LOGW(TAG, "Unhandled Ethernet Event: %d", event_id);
        break;
    }
}

static void got_ip_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&ip_info->ip));
}

void net_init(void) {
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    init_spi();

    esp_eth_mac_t *mac = eth_w5500_get_mac();
    esp_eth_phy_t *phy = eth_w5500_get_phy();

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, (void*)&MAC_ADDRESS));

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_config);
    esp_eth_netif_glue_handle_t eth_netif_glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, eth_netif_glue));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,  IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,    &eth_event_handler,    NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}
