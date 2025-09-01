#include "wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>

LOG_MODULE_REGISTER(WIFI);

static struct net_mgmt_event_callback wifi_cb;
static K_SEM_DEFINE(sem_wifi, 0, 1);

static struct net_mgmt_event_callback ipv4_cb;
static K_SEM_DEFINE(sem_ipv4, 0, 1);

static void on_wifi_conn_event(
    struct net_mgmt_event_callback *cb,
    uint64_t mgmt_event,
    struct net_if *iface
) {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        if (status->status) {
            LOG_ERR("Failed to connect to WiFi with status: %d", status->status);
        } else {
            LOG_INF("Successfully connected to WiFi");
            k_sem_give(&sem_wifi);
        }
    }
    else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        if (status->status) {
            LOG_ERR("Disconnect failed with status: %d", status->status);
        } else {
            LOG_INF("Successfully disconnected from WiFi");
            k_sem_take(&sem_wifi, K_NO_WAIT);
        }
    }
    else {
        LOG_WRN("Unhandled WiFi event: %lld", mgmt_event);
    }
}

static void on_ipv4_event(
    struct net_mgmt_event_callback *cb,
    uint64_t mgmt_event,
    struct net_if *iface
) {
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        LOG_INF("IPv4 address added");
        k_sem_give(&sem_ipv4);
    } 
    else if (mgmt_event == NET_EVENT_IPV4_ADDR_DEL) {
        LOG_INF("IPv4 address removed");
    }
    else if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
        LOG_INF("DHCP bound - IP address received");
        k_sem_give(&sem_ipv4);
    }
    else {
        LOG_WRN("Unhandled IPv4 event: %lld", mgmt_event);
    }
}

void wifi_init(void) {
    net_mgmt_init_event_callback(
        &wifi_cb,
        on_wifi_conn_event,
        NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT
    );

    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(
        &ipv4_cb,
        on_ipv4_event,
        NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL | NET_EVENT_IPV4_DHCP_BOUND
    );

    net_mgmt_add_event_callback(&ipv4_cb);
}

int wifi_connect(char *ssid, char *psk) {
    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t *)ssid,
        .ssid_length = strlen(ssid),
        .psk = (const uint8_t *)psk,
        .psk_length = strlen(psk),
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
        .channel = WIFI_CHANNEL_ANY,
        .mfp = WIFI_MFP_OPTIONAL
    };

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) return ret;

    LOG_INF("Waiting for WiFi connection...");
    k_sem_take(&sem_wifi, K_FOREVER);

    return 0;
}

int wifi_get_status(void) {
    struct net_if *iface = net_if_get_default();

    LOG_INF("Waiting for IP address...");
    k_sem_take(&sem_ipv4, K_FOREVER);

    struct wifi_iface_status status;
    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status))) {
        LOG_ERR("Failed to request WiFi status");
        return 1;
    }

    char ip_addr[NET_IPV4_ADDR_LEN] = {0};
    if (net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr, ip_addr, sizeof(ip_addr)) == NULL) {
        LOG_ERR("Failed to convert IP address to string");
        return 1;
    }

    LOG_INF("WiFi Status: %s", wifi_state_txt(status.state));
    if (status.state >= WIFI_STATE_ASSOCIATED) {
        LOG_INF("SSID: %s", status.ssid);
        LOG_INF("Band: %s", wifi_band_txt(status.band));
        LOG_INF("Channel: %d", status.channel);
        LOG_INF("Security: %s", wifi_security_txt(status.security));
        LOG_INF("IP address: %s", ip_addr);
    }

    return 0;
}
