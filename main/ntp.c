#include "ntp.h"

#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_netif_sntp.h>
#include <sys/time.h>

#define TAG "ntp"

void callback(struct timeval *tv) {
    time_t nowtime = tv->tv_sec;
    struct tm *nowtm = localtime(&nowtime);
    char buf[64];

    strftime(buf, sizeof(buf), "%c", nowtm);
    ESP_LOGI(TAG, "Time synchronized successfully. Current timestamp: %s", buf);
}

void ntp_init(void) {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.server_from_dhcp = true;
    config.renew_servers_after_new_IP = true;
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    config.index_of_first_server = 1;
    config.sync_cb = callback;

    esp_netif_sntp_init(&config);
}
