#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include "hardware.h"

void lock_trigger(void) {
    static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&spinlock);

    gpio_set_level(LOCK_GPIO, LOCK_OPENED_LOGIC_LEVEL);
    esp_rom_delay_us(LOCK_OPEN_TIME_US);
    gpio_set_level(LOCK_GPIO, !LOCK_OPENED_LOGIC_LEVEL);

    taskEXIT_CRITICAL(&spinlock);
}
