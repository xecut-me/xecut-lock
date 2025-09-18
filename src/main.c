#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/clock.h>
#include <zephyr/timing/timing.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <string.h>

#include "wifi.h"
#include "dns.h"
#include "sntp.h"
#include "otp.h"
#include "keypad.h"
#include "zephyr/irq.h"

LOG_MODULE_REGISTER(MAIN, 4);

// test
#include "mqtt.h"

// #define WIFI_SSID "CGA2121_7d2gR7y"
// #define WIFI_PSK  "N4fzQqD9Tsv7rjy6bp"
// #define WIFI_SSID "xecut"
// #define WIFI_PSK  "themostsecurepassword"
#define WIFI_SSID "MiFibra-BC64"
#define WIFI_PSK  "CXn45uUa"

// #define RELAY0_NODE DT_CHOSEN(xecut_relay0)
// static const struct gpio_dt_spec relay0 = GPIO_DT_SPEC_GET(RELAY0_NODE, gpios);

#include <mbedtls/pkcs5.h>
#define CONFIG_KDF_KEY_NAME "kdf_secret"
#define CONFIG_KDF_KEY_MAX_SIZE 16
#define CONFIG_KDF_ROUNDS 4000
#define CONFIG_OTP_KEY_SIZE 0x30
#define CONFIG_OTP_DIGITS 6
#define CONFIG_OTP_TIMESTEP 30
extern uint8_t kdf_key[CONFIG_KDF_KEY_MAX_SIZE];
extern size_t kdf_key_size;

void trigger_lock(){
    // unsigned int key = irq_lock();
    // gpio_pin_set_dt(&relay0, 1);
    // k_busy_wait(200000);
    // gpio_pin_set_dt(&relay0, 0);
    // irq_unlock(key);
}


bool command_test(uint8_t *cmd, size_t cmd_len) {
    // printf("key_size =%d \n cmd_len= %d\ncommand=%s\n",kdf_key_size cmd_len, cmd);
    // for (int i=0;i<kdf_key_size;i++){
    //     printf("%02x",kdf_key[i]);
    // }
    // printf("\n");
    // uint8_t otp_key[CONFIG_OTP_KEY_SIZE];
    // mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1, cmd, cmd_len, kdf_key, kdf_key_size, CONFIG_KDF_ROUNDS, sizeof(otp_key), otp_key);
    // for (int i=0;i<sizeof(otp_key);i++){
    //     printf("%02x",otp_key[i]);
    // }
    // printf("\ndone\n");
    // if (!strcmp(cmd,"TST")){
    //     trigger_lock();
    // }

    return 1;
}

bool checkin_test(uint8_t *uid, size_t uid_len, uint8_t *code, size_t code_len) {
    printf("checkin, uid=%s code=%s\n", uid, code);
    bool ret = otp_verify_kdf(uid,uid_len,code,code_len);
    printf("checkin, ret=%d\n", ret);
    if (ret) trigger_lock();
    return 1;
}

int main(void) {
    int ret;

    // if (!gpio_is_ready_dt(&relay0)) {
    //     return 0;
    // }

    // ret = gpio_pin_configure_dt(&relay0, GPIO_OUTPUT_INACTIVE);
    // if (ret < 0) {
    //     return 0;
    // }

    wifi_init(WIFI_SSID, WIFI_PSK);

    ret = wifi_connect();
    if (ret) {
        LOG_ERR("Failed to connect to WiFi: %d", ret);
        return 0;
    }
    
    // try_sync_time();

    // #define UART_NODE DT_CHOSEN(xecut_keypad)
    // const struct device *uart = DEVICE_DT_GET(UART_NODE);

    // struct keypad *kp = k_malloc(sizeof(struct keypad));
    // struct keypad_callbacks kp_callbacks = {
    //     .command = command_test,
    //     .checkin = checkin_test,
    // };
    // __ASSERT(kp != NULL, "cannot alloc kp");
    // keypad_init(uart, kp_callbacks, kp);
    // otp_init();

    mqtt_init("192.168.1.126", 1883, NULL, NULL);
    mqtt_run();

    for (;;) {
        mqtt_test();
        k_msleep(2500);
    }

    return 0;
}

