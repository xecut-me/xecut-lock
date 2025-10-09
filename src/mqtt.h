#pragma once

#include <stdint.h>

void mqtt_init(
    const char *ip_addr,
    uint16_t port,
    const char *user_name,
    const char *password
);

int mqtt_publish_msg(const char *topic, const char *payload, int qos);
