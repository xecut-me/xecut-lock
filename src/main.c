#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/clock.h>

#include "wifi.h"
#include "dns.h"
#include "sntp.h"

LOG_MODULE_REGISTER(MAIN);

#define WIFI_SSID "CGA2121_7d2gR7y"
#define WIFI_PSK  "N4fzQqD9Tsv7rjy6bp"

int main(void) {
    int ret;

    wifi_init();
    
    ret = wifi_connect(WIFI_SSID, WIFI_PSK);
    if (ret) {
        LOG_ERR("Failed to connect to WiFi: %d", ret);
        return 0;
    }
    
    ret = wifi_get_status();
    if (ret) {
        LOG_ERR("Failed to get WiFi status: %d", ret);
        return 0;
    }

    try_sync_time();

    for (;;) {
        struct timespec tp;
        int err = sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
        if (err) {
            LOG_ERR("Failed to get time");
            return 0;
        }

        uint32_t upt = k_uptime_seconds();

        printf("Uptime: %d, real time: %s\n", upt, ctime(&tp.tv_sec));

        k_msleep(1000);
    }

    return 0;
}
