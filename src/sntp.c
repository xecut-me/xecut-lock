#include "sntp.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/clock.h>

#include "dns.h"

LOG_MODULE_REGISTER(SNTP);

#define SNTP_HOST        "0.pool.ntp.org"
#define SNTP_PORT        (123)
#define SNTP_TIMEOUT_MS  (4000)

static K_SEM_DEFINE(sntp_async_received, 0, 1);

static void sntp_service_handler(struct net_socket_service_event *pev);

NET_SOCKET_SERVICE_SYNC_DEFINE_STATIC(service_sntp_async, sntp_service_handler, 1);

static void sntp_service_handler(struct net_socket_service_event *pev) {
    int ret;
    struct sntp_time time;

    ret = sntp_read_async(pev, &time);
    if (ret) {
        LOG_WRN("Failed to sync time: sntp_read_async failed with status code %d", ret);
        return;
    }

    sntp_close_async(&service_sntp_async);

	struct timespec tspec;
    tspec.tv_sec = time.seconds;
	tspec.tv_nsec = ((uint64_t)time.fraction * (1000 * 1000 * 1000)) >> 32;
	
    ret = sys_clock_settime(SYS_CLOCK_REALTIME, &tspec);
    if (ret) {
        LOG_WRN("Failed to sync time: sys_clock_settime failed with status code %d", ret);
        return;
    }

    k_sem_give(&sntp_async_received);
}

void try_sync_time(void) {
    int ret;

    struct sockaddr addr;
    socklen_t addrlen;
    struct sntp_ctx ctx;

    ret = dns_query(SNTP_HOST, SNTP_PORT, AF_INET, SOCK_DGRAM, &addr, &addrlen);
    if (ret) {
        LOG_WRN("Failed to sync time: failed to resolve IP address of NTP server");
        return;
    }

    ret = sntp_init_async(&ctx, &addr, addrlen, &service_sntp_async);
    if (ret) {
        LOG_WRN("Failed to sync time: sntp_init failed with status code %d", ret);
        goto end;
    }

    ret = sntp_send_async(&ctx);
    if (ret) {
        LOG_WRN("Failed to sync time: sntp_query failed with status code %d", ret);
        goto end;
    }

    ret = k_sem_take(&sntp_async_received, K_MSEC(SNTP_TIMEOUT_MS));
    if (ret) {
        LOG_WRN("Failed to sync time: request timeout");
        goto end;
    }

    struct timespec tp;
    sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
    LOG_INF("Time synced successfully! Current timestamp: %s (%lld)", ctime(&tp.tv_sec), tp.tv_sec);

end:
	sntp_close(&ctx);
}
