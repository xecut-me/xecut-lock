#include "indicator.h"

#include <esp_event.h>
#include <esp_wifi.h>
#include <led_strip.h>

static led_strip_handle_t led_strip = NULL;

void indicator_configure_pin(int pin) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = pin,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

static void event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        led_strip_set_pixel(led_strip, 0, 80, 0, 0);
        led_strip_refresh(led_strip);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        led_strip_set_pixel(led_strip, 0, 0, 10, 0);
        led_strip_refresh(led_strip);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        led_strip_set_pixel(led_strip, 0, 80, 0, 0);
        led_strip_refresh(led_strip);
    }
}

void indicator_listen_events(void) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
}
