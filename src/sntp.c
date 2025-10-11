#include "sntp.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/net/sntp.h>
#include <zephyr/sys/clock.h>
#include <zephyr/shell/shell.h>

#include <stdint.h>

#include "dns.h"

LOG_MODULE_REGISTER(SNTP);

#define SNTP_TIMEOUT_MS  2000

static struct {
    char host[128 + 1];
    uint16_t port;
    uint32_t last_sync;
} ntp = {0};

static int set_time(struct sntp_time time) {
    struct timespec ts;
    ts.tv_sec = time.seconds;
    ts.tv_nsec = ((uint64_t)time.fraction * (1000 * 1000 * 1000)) >> 32;

    return sys_clock_settime(SYS_CLOCK_REALTIME, &ts);
}

static void try_sync_time(void) {
    int ret;

    struct sockaddr addr;
    socklen_t addrlen;
    struct sntp_ctx ctx;
    struct sntp_time time;

    ret = dns_query(ntp.host, ntp.port, AF_INET, SOCK_DGRAM, &addr, &addrlen);
    if (ret) {
        LOG_ERR("Failed to sync time: failed to resolve IP address of NTP server");
        return;
    }

    ret = sntp_init(&ctx, &addr, addrlen);
    if (ret) {
        LOG_ERR("Failed to sync time: sntp_init failed with status code %d", ret);
        goto end;
    }

    ret = sntp_query(&ctx,SNTP_TIMEOUT_MS, &time);
    if (ret) {
        LOG_ERR("Failed to sync time: sntp_query failed with status code %d", ret);
        goto end;
    }

    ret = set_time(time);
    if (ret) {
        LOG_ERR("Failed to set time: set_time failed with status code %d", ret);
        goto end;
    }

    ntp.last_sync = k_uptime_get_32();

    struct timespec tp;
    sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
    printf("Time synced successfully! Current time: %s (%lld)\n", ctime(&tp.tv_sec), tp.tv_sec);

end:
    sntp_close(&ctx);
}

void ntp_init(void) {
    // TODO: Read settings here.

    strcpy((char*)&ntp.host, "0.pool.ntp.org");
    ntp.port = 123;
}

static int cmd_server(const struct shell *sh, size_t argc, char **argv) {
    if (argc < 3) {
        shell_error(sh, "Usage: ntp server HOST PORT\n");
        return 1;
    }

    if (strlen(argv[1]) > sizeof(ntp.host) - 1) {
        shell_error(sh, "Too long host\n");
        return 1;
    }

    // TODO: Save values to settings and call ntp_init(void) again.
    strcpy((char*)&ntp.host, argv[1]);
    ntp.port = atoi(argv[2]);

    shell_print(sh, "NTP server saved!\n");

    return 0;
}

static char* format_last_sync() {
    static char buf[24];

    const uint32_t diff = (k_uptime_get_32() - ntp.last_sync) / 1000;
    const uint32_t days = diff / (24 * 3600);
    const uint16_t hours = diff % (24 * 3600) / 3600;
    const uint16_t minutes = diff % 3600 / 60;
    const uint16_t seconds = diff % 60;

    snprintf(
        (char*)&buf, sizeof(buf),
        "%dd %dh %dm %ds",
        days, hours, minutes, seconds
    );

    return (char*)&buf;
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv) {
    struct timespec tp;
    sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
    shell_print(sh, "Current time: %s\n", ctime(&tp.tv_sec));

    if (ntp.host[0]) {
        shell_print(sh, "Host: %s:%d\n", ntp.host, ntp.port);
    } else {
        shell_print(sh, "Host: not set\n");
    }

    if (ntp.last_sync == 0) {
        shell_print(sh, "Time is not synchronized\n");
    } else {
        shell_print(sh, "Synced %s ago\n", format_last_sync());
    }

    return 0;
}

static int cmd_sync(const struct shell *sh, size_t argc, char **argv) {
    if (ntp.host[0] == 0) {
        shell_print(sh, "Host is not set!\n");
        return 1;
    }

    try_sync_time();
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntp,
    SHELL_CMD(server, NULL, "1", cmd_server),
    SHELL_CMD(status, NULL, "2", cmd_status),
    SHELL_CMD(sync, NULL, "3", cmd_sync),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ntp, &sub_ntp, "NTP commands", NULL);