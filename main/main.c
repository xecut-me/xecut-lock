#include <stdio.h>

#include <esp_log.h>
#include <esp_event.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "hardware.h"
#include "keypad.h"
#include "lock.h"
#include "ntp.h"
#include "network.h"
#include "mqtt.h"

#define TAG "main"

bool command(uint8_t *cmd, size_t cmd_len) {
    ESP_LOGI("TEST", "COMMAND %s", cmd);
    return 1;
}

bool checkin(uint8_t *uid, size_t uid_len, uint8_t *code, size_t code_len) {
    ESP_LOGI("TEST", "UID %s and CODE %s", uid, code);
    return 1;
}

void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    hardware_setup();

    keypad_init((struct keypad_callbacks){
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
