#include "indicator.h"

#include <esp_event.h>
#include <esp_wifi.h>
#include <mqtt_client.h>
#include <led_strip.h>

#include "mqtt.h"

typedef enum {
    INDICATOR_OK,
    INDICATOR_NO_MQTT,
    INDICATOR_NO_WIFI,
} indicator_status_t;

static struct {
    led_strip_handle_t led_strip;
    indicator_status_t status;
    bool show_setup_blink;
} indicator = {0};

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

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &indicator.led_strip));
}

static void set_led_color(uint32_t red, uint32_t green, uint32_t blue) {
    led_strip_set_pixel(indicator.led_strip, 0, red, green, blue);
    led_strip_refresh(indicator.led_strip);
}

static void display_current_status() {
    if (indicator.show_setup_blink) {
        return;
    }

    switch (indicator.status) {
        case INDICATOR_OK:
            set_led_color(0, 10, 0);
            break;
        case INDICATOR_NO_MQTT:
            set_led_color(80, 30, 0);
            break;
        case INDICATOR_NO_WIFI:
            set_led_color(80, 0, 0);
            break;
    }
}

static void set_status(indicator_status_t status) {
    indicator.status = status;
    display_current_status();
}

static void wifi_event_handler(
    void *event_handler_arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    if (event_id == WIFI_EVENT_STA_START) {
        set_status(INDICATOR_NO_WIFI);
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        set_status(INDICATOR_NO_MQTT);
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        set_status(INDICATOR_NO_WIFI);
    }
}

static void mqtt_event_handler(
    void *event_handler_arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    if (event_id == MQTT_EVENT_CONNECTED) {
        set_status(INDICATOR_OK);
    }
    else if (event_id == MQTT_EVENT_DISCONNECTED && indicator.status != INDICATOR_NO_WIFI) {
        set_status(INDICATOR_NO_MQTT);
    }
}

static void start_listen_events(void) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_get_client(), ESP_EVENT_ANY_ID, &mqtt_event_handler, NULL));
}

static void indicator_thread(void *param) {
    while (indicator.show_setup_blink) {
        set_led_color(0, 0, 100);
        vTaskDelay(pdMS_TO_TICKS(1000));
        set_led_color(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    display_current_status();

    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}

void indicator_init(void) {
    indicator.status = INDICATOR_NO_WIFI;
    indicator.show_setup_blink = true;

    start_listen_events();

    xTaskCreate(
        indicator_thread,
        "indicator",
        4096,
        NULL,
        tskIDLE_PRIORITY,
        NULL
    );
}

void indicator_setup_complete(void) {
    indicator.show_setup_blink = false;
}
