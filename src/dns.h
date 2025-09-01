#pragma once

#include <stdint.h>
#include <zephyr/net/net_ip.h>

int dns_query(
    const char *host,
    uint16_t port,
    int family,
    int socktype,
    struct sockaddr *addr,
	socklen_t *addrlen
);