#include <esp_log.h>
#include <esp_timer.h>
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
#include "indicator.h"

#ifdef USE_WIFI
#include "wifi.h"
#else
#include "ethernet.h"
#endif

#define TAG "main"

#define UPTIME()  (esp_timer_get_time() / 1000000)

time_t start_timestamp;

void save_start_timestamp() {
    start_timestamp = UPTIME();
}

bool command(const char *cmd) {
    const char *topic = MQTT_TOPIC(MQTT_DEVICE_ID, "command");

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

    const char *topic = MQTT_TOPIC(MQTT_DEVICE_ID, "checkin");

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
    keypad_process(data, data_len);
}

void status_thread(void *param) {
    for (;;) {
        const char *topic = MQTT_TOPIC(MQTT_DEVICE_ID, "status");

        const char *status = "alive";

        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        int64_t uptime = UPTIME() - start_timestamp;

        char message[128] = {0};
        snprintf(
            (char*)&message, sizeof(message),
            "{\"status\": \"%s\", \"timestamp\": \"%lld\", \"uptime\": %lld}",
            status, tv_now.tv_sec, uptime
        );

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
    uint8_t *buffer = (uint8_t*)malloc(KEYPAD_UART_BUFFER_SIZE);
    int64_t last_input_timestamp = INT64_MAX;

    for (;;) {
        int len = uart_read_bytes(
            KEYPAD_UART_NUM,
            buffer,
            KEYPAD_UART_BUFFER_SIZE,
            pdMS_TO_TICKS(20)
        );

        // Reset keypad after inactivity before receiving new data.
        if (len == 0) {
            bool keypad_used = last_input_timestamp != INT64_MAX;
            int64_t inaction_time = esp_timer_get_time() - last_input_timestamp;
            int64_t us_to_reset = 30 * 1000 * 1000;  // 30 seconds

            if (keypad_used && inaction_time >= us_to_reset) {
                ESP_LOGD(TAG, "Reset keypad after 30 seconds of inactivity");
                keypad_reset();
                last_input_timestamp = INT64_MAX;
            }

            continue;
        }

        keypad_process((char*)buffer, len);
        last_input_timestamp = esp_timer_get_time();
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    save_start_timestamp();

    hardware_setup();

    ntp_init();

#ifdef USE_WIFI
    wifi_init();
#else
    eth_init();
#endif

    // TODO: wait first connect before trying to connect to mqtt server.
    mqtt_init();
    mqtt_subscribe(MQTT_TOPIC(MQTT_DEVICE_ID, "lock"), /* qos */ 1, mqtt_lock_topic_updated);
    run_status_thread();

    indicator_init();

    keypad_init((struct keypad_callbacks) {
        .checkin = checkin,
        .command = command,
    });

    keypad_loop();
}
