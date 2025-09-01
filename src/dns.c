#include "dns.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(DNS);

int dns_query(
    const char *host,
    uint16_t port,
    int family,
    int socktype,
    struct sockaddr *addr,
	socklen_t *addrlen
) {
    int ret;

    struct addrinfo hints = {
        .ai_family = family,
        .ai_socktype = socktype,
    };

    struct addrinfo *res = NULL;

    ret = getaddrinfo(host, NULL, &hints, &res);
    if (ret) {
        LOG_ERR("getaddrinfo failed with status code %d (errno %d)", ret, errno);
        return 1;
    }

    *addr = *res->ai_addr;
    *addrlen = res->ai_addrlen;

    freeaddrinfo(res);

    net_sin(addr)->sin_port = htons(port);

    char addr_str[INET6_ADDRSTRLEN] = {0};
    inet_ntop(addr->sa_family, &net_sin(addr)->sin_addr, addr_str, sizeof(addr_str));

    return 0;
}