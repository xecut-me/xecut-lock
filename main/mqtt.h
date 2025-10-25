#pragma once

#define MQTT_TOPIC(device_id, topic) ("xecut-lock/" device_id "/" topic)

typedef void (*mqtt_topic_updated_handler_t)(
    const char *topic, int topic_len,
    const char *data,  int data_len
);

void mqtt_init(void);

int mqtt_publish(const char *topic, const char *payload, int qos, int retain);

int mqtt_subscribe(const char *topic, int qos, mqtt_topic_updated_handler_t callback);
