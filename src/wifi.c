#include "wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>

LOG_MODULE_REGISTER(WIFI);

#define RECONNECT_DELAY  K_SECONDS(10)

enum wifi_event {
    WIFI_EVENT_SETUP_COMPLETE,
    WIFI_EVENT_CONNECTED,
    WIFI_EVENT_CONNECTION_FAILED,
    WIFI_EVENT_DISCONNECTED,
    WIFI_EVENT_IP_ADDRESS_RECEIVED,
};

static struct {
    bool init;

    char *ssid;
    char *psk;

    struct net_mgmt_event_callback conn_cb;
    struct net_mgmt_event_callback ipv4_cb;

    k_tid_t tid;

    char evtq_buffer[4 * sizeof(enum wifi_event)];
    struct k_msgq evtq;
} wifi = {0};

K_THREAD_STACK_DEFINE(wifi_thread_stack_area, 2048);
const int wifi_thread_priority = 2;
struct k_thread wifi_thread_data;

void start_wifi_thread(void);
void wifi_thread(void *unused1, void *unused2, void *unused3);

static void on_wifi_conn_event(
    struct net_mgmt_event_callback *cb,
    uint64_t mgmt_event,
    struct net_if *iface
) {
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    enum wifi_event event;

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        if (status->status) {
            LOG_WRN("Failed to connect to WiFi with status code %d", status->status);
            event = WIFI_EVENT_CONNECTION_FAILED;
        } else {
            LOG_DBG("Successfully connected to WiFi");
            event = WIFI_EVENT_CONNECTED;
        }
    }
    else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        // this is event+status combination maybe esp32s3 specific
        if (status->status == -1) {
            event = WIFI_EVENT_CONNECTION_FAILED;
        } else {
            event = WIFI_EVENT_DISCONNECTED;
        }
    }
    else {
        LOG_WRN("Unhandled WiFi event: %lld", mgmt_event);
        return;
    }

    k_msgq_put(&wifi.evtq, &event, K_FOREVER);
}

static void on_ipv4_event(
    struct net_mgmt_event_callback *cb,
    uint64_t mgmt_event,
    struct net_if *iface
) {
    enum wifi_event event;

    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        LOG_DBG("IPv4 address added");
        return;
    } 
    else if (mgmt_event == NET_EVENT_IPV4_ADDR_DEL) {
        LOG_DBG("IPv4 address removed");
        return;
    }
    else if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
        LOG_DBG("DHCP bound - IP address received");
        event = WIFI_EVENT_IP_ADDRESS_RECEIVED;
    }
    else {
        LOG_WRN("Unhandled IPv4 event: %lld", mgmt_event);
    }

    k_msgq_put(&wifi.evtq, &event, K_FOREVER);
}

int wifi_connect(void) {
    struct net_if *iface = net_if_get_default();

    struct wifi_connect_req_params params = {
        .ssid = (const uint8_t *)wifi.ssid,
        .ssid_length = strlen(wifi.ssid),
        .psk = (const uint8_t *)wifi.psk,
        .psk_length = strlen(wifi.psk),
        .security = WIFI_SECURITY_TYPE_PSK,
        .band = WIFI_FREQ_BAND_2_4_GHZ,
        .channel = WIFI_CHANNEL_ANY,
        .mfp = WIFI_MFP_OPTIONAL
    };

    return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
}

int wifi_disconnect(void) {
    struct net_if *iface = net_if_get_default();

    struct wifi_iface_status status;
    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status))) {
        LOG_ERR("Failed to request WiFi status");
        return EIO;
    }

    if (status.state < WIFI_STATE_AUTHENTICATING) {
        // wifi not connected, nothing to do
        return ENOTCONN;
    }

    if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0)) {
        LOG_ERR("Failed to send WiFi disconnect request");
        return EIO;
    }

    return 0;
}

int wifi_init(char *ssid, char *psk) {
    wifi.ssid = ssid;
    wifi.psk = psk;

    const int status = wifi_disconnect();
    if (status == 0) return 0;
    if (status != ENOTCONN) return status;

    net_mgmt_init_event_callback(
        &wifi.conn_cb,
        on_wifi_conn_event,
        NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT
    );

    net_mgmt_add_event_callback(&wifi.conn_cb);

    net_mgmt_init_event_callback(
        &wifi.ipv4_cb,
        on_ipv4_event,
        NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL | NET_EVENT_IPV4_DHCP_BOUND
    );

    net_mgmt_add_event_callback(&wifi.ipv4_cb);

    if (!wifi.init) {
        k_msgq_init(
            &wifi.evtq,
            (char*)&wifi.evtq_buffer,
            sizeof(enum wifi_event),
            sizeof(wifi.evtq_buffer) / sizeof(enum wifi_event)
        );

        start_wifi_thread();
    }

    enum wifi_event wifi_event = WIFI_EVENT_SETUP_COMPLETE;
    k_msgq_put(&wifi.evtq, &wifi_event, K_FOREVER);

    wifi.init = true;

    return 0;
}

void start_wifi_thread(void) {
    if (wifi.tid) return;

    wifi.tid = k_thread_create(
        &wifi_thread_data, wifi_thread_stack_area,
        K_THREAD_STACK_SIZEOF(wifi_thread_stack_area),
        wifi_thread,
        NULL, NULL, NULL,
        wifi_thread_priority, 0, K_NO_WAIT);
}

void wifi_thread(void *unused1, void *unused2, void *unused3) {
    int ret;
    enum wifi_event event;

    for (;;) {
        k_msgq_get(&wifi.evtq, &event, K_FOREVER);
        LOG_DBG("Received WiFi event: %d", event);

        switch (event) {
            case WIFI_EVENT_CONNECTED:
                LOG_INF("Successfully connected to WiFi!");
                break;

            case WIFI_EVENT_DISCONNECTED:
                LOG_WRN("WiFi is disconnected...");

            case WIFI_EVENT_CONNECTION_FAILED:
                k_sleep(RECONNECT_DELAY);

            case WIFI_EVENT_SETUP_COMPLETE:
                ret = wifi_connect();
                if (ret) {
                    LOG_WRN("Failed to send connection request: %d", ret);
                    k_msgq_put(&wifi.evtq, &event, K_FOREVER);
                }

                break;

            case WIFI_EVENT_IP_ADDRESS_RECEIVED:
                // Ignore for now

            default:
                break;
        }
    }
}