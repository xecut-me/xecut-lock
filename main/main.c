#include <stdio.h>

#include <esp_log.h>
#include <esp_event.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

#include "config.h"
#include "hardware.h"
#include "keypad.h"
#include "otp.h"
#include "lock.h"
#include "network.h"
#include "ntp.h"
#include "mqtt.h"

#define TAG "main"

bool command(const char *cmd) {
    const char *topic = MQTT_CLIENT_ID "/command";

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    char message[256] = {0};
    snprintf((char*)&message, sizeof(message), "{\"command\": \"%s\", \"timestamp\": \"%lld\"}", cmd, tv_now.tv_sec);

    mqtt_publish(topic, message, /* qos */ 1, /* retain */ false);

    return true;
}

bool checkin(const char *uid, const char *code) {
    bool is_valid_otp = otp_verify(uid, code);
    if (!is_valid_otp) return false;

    const char *topic = MQTT_CLIENT_ID "/checkin";

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    char message[256] = {0};
    snprintf((char*)&message, sizeof(message), "{\"uid\": \"%s\", \"timestamp\": \"%lld\"}", uid, tv_now.tv_sec);

    mqtt_publish(topic, message, /* qos */ 1, /* retain */ false);

    return true;
}

void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    hardware_setup();

    keypad_init((struct keypad_callbacks) {
        .checkin = checkin,
        .command = command,
    });

    ntp_init();
    net_init();
    mqtt_init();

    uint8_t *keypad_buffer = (uint8_t*)malloc(KEYPAD_UART_BUFFER_SIZE);
    for (;;) {
        int len = uart_read_bytes(
            KEYPAD_UART_NUM,
            keypad_buffer,
            KEYPAD_UART_BUFFER_SIZE-1,
            pdMS_TO_TICKS(20)
        );

        if (!len) continue;

        keypad_buffer[len] = '\0';

        keypad_process((const char*)keypad_buffer);
    }
}
