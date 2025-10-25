#include "mqtt.h"

#include <esp_log.h>
#include <esp_err.h>
#include <mqtt_client.h>

#include "config.h"

#define TAG "mqtt"

struct mqtt_subscription {
    const char *topic;
    int qos;
    mqtt_topic_updated_handler_t callback;
};

static struct {
    esp_mqtt_client_handle_t client;
    bool connected;

    struct mqtt_subscription subscriptions[MQTT_SUBSCRIPTIONS_LIMIT];
    int subs_count;
} mqtt = {0};

void subscribe_mqtt_topics() {
    for (int i = 0; i < MQTT_SUBSCRIPTIONS_LIMIT; i++) {
        struct mqtt_subscription sub = mqtt.subscriptions[i];
        if (sub.topic == NULL) break;

        int res = esp_mqtt_client_subscribe_single(mqtt.client, sub.topic, sub.qos);
        if (res < 0) {
            ESP_LOGE(TAG, "Failed to subscribe to topic '%s'", sub.topic);
        } else {
            ESP_LOGI(TAG, "Successfully subscribed to topic '%s'", sub.topic);
        }
    }
} 

void notify_mqtt_subscriber(
    const char *topic, int topic_len,
    const char *data,  int data_len
) {
    for (int i = 0; i < MQTT_SUBSCRIPTIONS_LIMIT; i++) {
        struct mqtt_subscription sub = mqtt.subscriptions[i];
        if (sub.topic == NULL) break;

        if (strlen(sub.topic) != topic_len) {
            continue;
        }

        if (memcmp(sub.topic, topic, topic_len) == 0) {
            sub.callback(topic, topic_len, data, data_len);
        }
    }
}

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
        mqtt.connected = true;
        subscribe_mqtt_topics();
        break;
    case MQTT_EVENT_DISCONNECTED:
        // MQTT client emit disconnected event even after unsuccessful reconnects.
        if (mqtt.connected) {
            ESP_LOGW(TAG, "Client disconnected from server");
            mqtt.connected = false;
        }
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Client transport error");
        }
        break;
    case MQTT_EVENT_DATA:
        notify_mqtt_subscriber(
            event->topic, event->topic_len,
            event->data,  event->data_len
        );
        break;
    default:
        ESP_LOGD(TAG, "Unhandled MQTT event %d", event_id);
        break;
    }
}

void mqtt_init(void) {
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.client_id = "xecut-lock-" MQTT_DEVICE_ID,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .network.reconnect_timeout_ms = MQTT_RECONNECT_DELAY_SEC * 1000,
    };
    mqtt.client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt.client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt.client);
}

int mqtt_publish(const char *topic, const char *payload, int qos, int retain) {
    if (!mqtt.connected) {
        ESP_LOGE(
            TAG, "Unable to publish message '%s' to topic '%s' without connection to server",
            topic, payload
        );
        return -1;
    }

    int status = esp_mqtt_client_publish(mqtt.client, topic, payload, 0, qos, retain);
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

int mqtt_subscribe(const char *topic, int qos, mqtt_topic_updated_handler_t callback) {
    if (mqtt.subs_count == MQTT_SUBSCRIPTIONS_LIMIT) return 1;

    mqtt.subscriptions[mqtt.subs_count].topic = topic;
    mqtt.subscriptions[mqtt.subs_count].qos = qos;
    mqtt.subscriptions[mqtt.subs_count].callback = callback;
    mqtt.subs_count += 1;

    return 0;
}
