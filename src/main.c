#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/clock.h>
#include <zephyr/timing/timing.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <string.h>

#include "wifi.h"
#include "dns.h"
#include "sntp.h"
#include "otp.h"
#include "keypad.h"

LOG_MODULE_REGISTER(MAIN);

// #define WIFI_SSID "CGA2121_7d2gR7y"
// #define WIFI_PSK  "N4fzQqD9Tsv7rjy6bp"
#define WIFI_SSID "xecut"
#define WIFI_PSK  "themostsecurepassword"

int main(void) {
    int ret;

    wifi_init(WIFI_SSID, WIFI_PSK);
    
    ret = wifi_connect();
    if (ret) {
        LOG_ERR("Failed to connect to WiFi: %d", ret);
        return 0;
    }

    for (;;) {
        k_msleep(2500);
    }

    return 0;
}
