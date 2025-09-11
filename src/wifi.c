#include "wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>

LOG_MODULE_REGISTER(WIFI);

#define CONNECT_TIMEOUT  K_SECONDS(15)
#define IPADDR_TIMEOUT   K_SECONDS(5)
#define RECONNECT_DELAY  K_SECONDS(10)

static struct {
    char *ssid;
    char *psk;
} wifi_creds = { NULL, NULL };

static struct net_mgmt_event_callback wifi_cb;
static K_SEM_DEFINE(sem_wifi, 0, 1);

static struct net_mgmt_event_callback ipv4_cb;
static K_SEM_DEFINE(sem_ipv4, 0, 1);

static int wifi_wait_for_ip_address(void);

static void wifi_reconnect(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(wifi_reconnect_work, wifi_reconnect);

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
            LOG_INF("Successfully connected to WiFi '%s'", wifi_creds.ssid);
            k_sem_give(&sem_wifi);
        }
    }
    else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        k_sem_take(&sem_wifi, K_NO_WAIT);
        k_work_schedule(&wifi_reconnect_work, RECONNECT_DELAY);
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
        LOG_DBG("IPv4 address added");
        k_sem_give(&sem_ipv4);
    } 
    else if (mgmt_event == NET_EVENT_IPV4_ADDR_DEL) {
        LOG_DBG("IPv4 address removed");
        k_sem_take(&sem_ipv4, K_NO_WAIT);
    }
    else if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
        LOG_DBG("DHCP bound - IP address received");
        k_sem_give(&sem_ipv4);
    }
    else {
        LOG_WRN("Unhandled IPv4 event: %lld", mgmt_event);
    }
}

void wifi_init(char *ssid, char *psk) {
    __ASSERT(wifi_creds.ssid == NULL, "WiFi SSID is already initialized");
    wifi_creds.ssid = ssid;

    __ASSERT(wifi_creds.psk == NULL, "WiFi PSK is already initialized");
    wifi_creds.psk = psk;

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

int wifi_connect(void) {
    int ret;

    struct net_if *iface = net_if_get_default();

    struct wifi_iface_status status;
    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status))) {
        LOG_ERR("Failed to request WiFi status");
        return EIO;
    }

    if (status.state >= WIFI_STATE_AUTHENTICATING) {
        LOG_WRN("Unable to initiate a connection to the network because a connection is currently being established");
        return 0;
    }

    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t *)wifi_creds.ssid,
        .ssid_length = strlen(wifi_creds.ssid),
        .psk = (const uint8_t *)wifi_creds.psk,
        .psk_length = strlen(wifi_creds.psk),
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
        .channel = WIFI_CHANNEL_ANY,
        .mfp = WIFI_MFP_OPTIONAL
    };

    LOG_DBG("Sending WiFi connect request");
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) return ret;

    LOG_INF("Waiting for WiFi connection...");
    ret = k_sem_take(&sem_wifi, CONNECT_TIMEOUT);
    if (ret) return ETIMEDOUT;

    LOG_INF("Waiting for IP address...");
    ret = wifi_wait_for_ip_address();
    if (ret) LOG_WRN("Failed to obtain IP address! Status code: %s (%d)", strerror(ret), ret);

    return 0;
}

static int wifi_wait_for_ip_address(void) {
    int ret = k_sem_take(&sem_ipv4, IPADDR_TIMEOUT);
    if (ret) {
        LOG_ERR("Timeout while waiting for IP address");
        return ETIMEDOUT;
    }

    struct net_if *iface = net_if_get_default();

    struct wifi_iface_status status;
    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status))) {
        LOG_ERR("Failed to request WiFi status");
        return EIO;
    }

    char ip_addr[NET_IPV4_ADDR_LEN] = {0};
    if (net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr, ip_addr, sizeof(ip_addr)) == NULL) {
        LOG_ERR("Failed to convert IP address to string");
        return ENOTSUP;
    }

    LOG_INF("Device IP address is %s", ip_addr);

    return 0;
}

static void wifi_reconnect(struct k_work *work) {
    int ret = wifi_connect();
    if (ret) {
        LOG_ERR("Failed to reconnect to WiFi '%s' with status %s (%d)", wifi_creds.ssid, strerror(ret), ret);
    }

    // After an unsuccessful connection attempt on the ESP32S3, the event
    // NET_EVENT_WIFI_DISCONNECT_RESULT will occur with a status=-1,
    // and a new connection attempt will take place in the function on_wifi_conn_event.
}
