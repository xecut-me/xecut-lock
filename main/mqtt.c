#include "mqtt.h"

#include <esp_log.h>
#include <esp_err.h>
#include <mqtt_client.h>

#include "config.h"

#define TAG "mqtt"

static esp_mqtt_client_handle_t client = NULL;
static bool client_connected = false;

void mqtt_event_handler(
    void* event_handler_arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Client connected to server");
        client_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        // MQTT client emit disconnected event even after unsuccessful reconnects.
        if (client_connected) {
            ESP_LOGW(TAG, "Client disconnected from server");
            client_connected = false;
        }
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Client transport error");
        }
        break;
    default:
        ESP_LOGD(TAG, "Unhandled MQTT event %d", event_id);
        break;
    }
}

void mqtt_init(void) {
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .network.reconnect_timeout_ms = MQTT_RECONNECT_DELAY_SEC * 1000,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

int mqtt_publish(const char *topic, const char *payload, int qos, int retain) {
    if (!client_connected) {
        ESP_LOGE(
            TAG, "Unable to publish message '%s' to topic '%s' without connection to server",
            topic, payload
        );
        return -1;
    }

    int status = esp_mqtt_client_publish(client, topic, payload, 0, qos, retain);
    if (status >= 0) {
        ESP_LOGI(
            TAG, "Successfully publish message '%s' to topic '%s' with qos=%d, retain=%d",
            topic, payload,
            qos, retain
        );
    } else {
        ESP_LOGE(
            TAG, "Failed to publish message '%s' to topic '%s': %s",
            topic, payload,
            status == -1 ? "unknown error" : "full outbox"
        );
    }

    return status;
}
