#pragma once

void mqtt_init(void);

int mqtt_publish(const char *topic, const char *payload, int qos, int retain);
