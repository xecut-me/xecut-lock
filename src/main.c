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

bool command_test(uint8_t *cmd, size_t cmd_len) {
    printf("command: %s\n", cmd);
    return 1;
}

bool checkin_test(uint8_t *uid, size_t uid_len, uint8_t *code, size_t code_len) {
    printf("checkin, uid=%s code=%s\n", uid, code);
    return 1;
}

int main(void) {
    int ret;

    // wifi_init();
    
    // ret = wifi_connect(WIFI_SSID, WIFI_PSK);
    // if (ret) {
    //     LOG_ERR("Failed to connect to WiFi: %d", ret);
    //     return 0;
    // }
    
    // ret = wifi_get_status();
    // if (ret) {
    //     LOG_ERR("Failed to get WiFi status: %d", ret);
    //     return 0;
    // }

    #define UART_NODE DT_NODELABEL(uart0)
    const struct device *uart = DEVICE_DT_GET(UART_NODE);

    struct keypad kp;
    struct keypad_callbacks kp_callbacks = {
        .command = command_test,
        .checkin = checkin_test,
    };

    keypad_init(uart, kp_callbacks, &kp);

    for (;;) {
        enum keypad_status status = keypad_poll(&kp);
        if (status != KEYPAD_STATUS_EMPTY_UART) {
            printf("keypad_poll return status %s, current state is %s\n", keypad_status_txt(status), keypad_state_txt(kp.state));
        }

        // Give some time for kernel
        k_yield();
    }

    // try_sync_time();

    // for (;;) {
        // struct timespec tp;
        // int err = sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
        // if (err) {
        //     LOG_ERR("Failed to get time");
        //     return 0;
        // }

        // uint32_t upt = k_uptime_seconds();

        // printf("Uptime: %d, real time: %s\n", upt, ctime(&tp.tv_sec));

        // struct timespec time;
        // sys_clock_gettime(SYS_CLOCK_REALTIME, &time);

        // timing_start();

        // timing_t start, end;
        // start = timing_counter_get();

        // // char *uid = "";
        // // size_t uid_len = strlen(uid);
        // // uint8_t key[128] = {0};
        // const char *raw_key = "QFACPADFSBJBSIGKAAJ4MKAPVIGZGZOP";
        // uint8_t key[128] = {0};
        // size_t key_len = base32_decode(raw_key, &key, sizeof(key));
        // // uint8_t key_len = 0x30;
        // // uint8_t kdf_key[16] = {0};
        // uint8_t digits = 6;
        // uint64_t step = time.tv_sec / 30;

        // // mbedtls_pkcs5_pbkdf2_hmac_ext(
        // //     MBEDTLS_MD_SHA1,
        // //     uid, uid_len,
        // //     kdf_key, sizeof(kdf_key-1),
        // //     4000,
        // //     key_len, key
        // // );

        // uint32_t otp = _get_otp(key, key_len, digits, step);

        // end = timing_counter_get();

        // timing_stop();

        // LOG_INF("OTP %d", otp);
        // LOG_INF("Time: %lld ns", timing_cycles_to_ns(timing_cycles_get(&start, &end)));

        // k_msleep(2500);
    // }

    return 0;
}
