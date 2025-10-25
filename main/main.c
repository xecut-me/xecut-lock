#include <esp_log.h>
#include <esp_event.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <sys/time.h>

#include "config.h"
#include "hardware.h"
#include "keypad.h"
#include "otp.h"
#include "lock.h"
#include "ntp.h"
#include "mqtt.h"

#ifdef USE_WIFI
#include "wifi.h"
#else
#include "ethernet.h"
#endif

#define TAG "main"

bool command(const char *cmd) {
    const char *topic = MQTT_TOPIC(MQTT_CLIENT_ID, "command");

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

    lock_trigger();

    const char *topic = MQTT_TOPIC(MQTT_CLIENT_ID, "checkin");

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    char message[256] = {0};
    snprintf((char*)&message, sizeof(message), "{\"uid\": \"%s\", \"timestamp\": \"%lld\"}", uid, tv_now.tv_sec);

    mqtt_publish(topic, message, /* qos */ 1, /* retain */ false);

    return true;
}

void mqtt_lock_topic_updated(
    const char *topic, int topic_len,
    const char *data,  int data_len
) {
    ESP_LOGI(TAG, "Some message received to /lock topic");
}

void status_thread(void *param) {
    for (;;) {
        const char *topic = MQTT_TOPIC(MQTT_CLIENT_ID, "status");

        const char *status = "alive";

        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        char message[128] = {0};
        snprintf((char*)&message, sizeof(message), "{\"status\": \"%s\", \"timestamp\": \"%lld\"}", status, tv_now.tv_sec);

        int ret = mqtt_publish(topic, message, /* qos */ 1, /* retain */ false);
        if (ret >= 0) {
            vTaskDelay(pdMS_TO_TICKS(10 * 1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(MQTT_RECONNECT_DELAY_SEC * 1000));
        }
    }
}

void run_status_thread(void) {
    xTaskCreate(
        status_thread,
        "lock_status",
        4096,
        NULL,
        tskIDLE_PRIORITY,
        NULL
    );
}

void keypad_loop(void) {
    keypad_init((struct keypad_callbacks) {
        .checkin = checkin,
        .command = command,
    });

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

void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    hardware_setup();

    ntp_init();

#ifdef USE_WIFI
    wifi_init();
#else
    eth_init();
#endif

    // TODO: wait first connect before trying to connect to mqtt server.
    mqtt_init();
    mqtt_subscribe(MQTT_TOPIC(MQTT_CLIENT_ID, "lock"), /* qos */ 1, mqtt_lock_topic_updated);
    run_status_thread();

    keypad_loop();
}
