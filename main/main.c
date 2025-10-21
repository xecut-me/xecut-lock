#include <stdio.h>

#include <esp_log.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "hardware.h"
#include "keypad.h"
#include "ntp.h"
#include "network.h"

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

    // keypad_init((struct keypad_callbacks){
    //     .checkin = checkin,
    //     .command = command,
    // });

    ntp_init();
    net_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
}
