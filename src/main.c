#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/clock.h>
#include <zephyr/timing/timing.h>

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <string.h>

#include "wifi.h"
#include "dns.h"
#include "sntp.h"
#include "otp.h"

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

    struct timespec time;
    sys_clock_gettime(SYS_CLOCK_REALTIME, &time);

    timing_start();

    timing_t start, end;
    start = timing_counter_get();

    char *uid = "";
    size_t uid_len = 0;
    uint8_t key[128] = {0};
    uint8_t key_len = 0x30;
    uint8_t kdf_key[16] = {0};
    uint8_t digits = 6;
    uint64_t step = time.tv_sec / 30;

    mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA1,
        uid, uid_len,
        kdf_key, sizeof(kdf_key),
        4000,
        key_len, key
    );

    uint32_t otp = _get_otp(key, key_len, digits, step);

    end = timing_counter_get();

    timing_stop();

    LOG_INF("OTP %d", otp);
    LOG_INF("Time: %lld ns", timing_cycles_to_ns(timing_cycles_get(&start, &end)));

    // for (;;) {
    //     struct timespec tp;
    //     int err = sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
    //     if (err) {
    //         LOG_ERR("Failed to get time");
    //         return 0;
    //     }

    //     uint32_t upt = k_uptime_seconds();

    //     printf("Uptime: %d, real time: %s\n", upt, ctime(&tp.tv_sec));

    //     k_msleep(1000);
    // }

    return 0;
}
