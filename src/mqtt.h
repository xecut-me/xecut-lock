#pragma once

#include <stdint.h>

void mqtt_init(
    const char *ip_addr,
    uint16_t port,
    const char *user_name,
    const char *password
);

void mqtt_run(void);

void mqtt_test(void);
