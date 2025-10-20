#include <stdio.h>

#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ntp.h"
#include "network.h"

void app_main(void) {
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ntp_init();
    net_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
}
