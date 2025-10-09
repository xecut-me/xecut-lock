#include "mqtt.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/posix/poll.h>
#include <zephyr/random/random.h>

#include <stdio.h>

LOG_MODULE_REGISTER(MQTT);

#define CLIENT_ID "xecut_lock"
#define RECONNECT_DELAY  K_SECONDS(10)

enum mqtt_event {
    MQTT_EVENT_SETUP_COMPLETE,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_CONNECTION_FAILED,
    MQTT_EVENT_DISCONNECTED,
};

static void client_setup(const char *user_name, const char *password);
static void client_broker_setup(const char *ip_addr, uint16_t port);

static void mqtt_evt_handler(
    struct mqtt_client *client,
    const struct mqtt_evt *evt
);

static struct {
    bool init;

    struct sockaddr_storage broker;
    struct mqtt_client client;
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];

    k_tid_t tid;

    char evtq_buffer[4 * sizeof(enum mqtt_event)];
    struct k_msgq evtq;
} mqtt = {0};

K_THREAD_STACK_DEFINE(mqtt_thread_stack_area, 2048);
const int mqtt_thread_priority = 8;
struct k_thread mqtt_thread_data;

static void setup_mqtt_evtq(void);
static void start_mqtt_thread(void);
static void mqtt_thread(void *unused1, void *unused2, void *unused3);

static void mqtt_evt_handler(
    struct mqtt_client *client,
    const struct mqtt_evt *evt
) {
    const enum mqtt_evt_type mqtt_event = evt->type;

    enum mqtt_event event;

    if (mqtt_event == MQTT_EVT_CONNACK) {
        if (evt->result) {
            LOG_WRN("Failed to connect to MQTT with status code %d", evt->result);
            event = MQTT_EVENT_CONNECTION_FAILED;
        } else {
            LOG_DBG("Successfully connected to MQTT");
            event = MQTT_EVENT_CONNECTED;
        }
    }
    else if (mqtt_event == MQTT_EVT_DISCONNECT) {
        event = MQTT_EVENT_DISCONNECTED;
    }
    else if (mqtt_event == MQTT_EVT_PUBREC) {
        if (evt->result != 0) {
            LOG_WRN("MQTT PUBREC error %d", evt->result);
            return;
        }

        const struct mqtt_pubrel_param rel_param = {
            .message_id = evt->param.pubrec.message_id
        };

        int ret = mqtt_publish_qos2_release(client, &rel_param);
        if (ret != 0) {
            LOG_ERR("mqtt_publish_qos2_release failed with status code %d", ret);
        }

        return;
    }

    k_msgq_put(&mqtt.evtq, &event, K_FOREVER);
}

static int client_connect(void) {
    int ret = mqtt_connect(&mqtt.client);
    if (ret) {
        LOG_WRN("Failed to connect to MQTT broker with status code %d", ret);
        return ret;
    }

    struct pollfd pollfd = { .fd = mqtt.client.transport.tcp.sock, .events = POLLIN };
    ret = poll(&pollfd, 1, 2000);
    if (ret < 0) {
        LOG_WRN("Failed to poll MQTT broker with status code %d", ret);
        return ret;
    }

    mqtt_input(&mqtt.client);

    enum mqtt_event event;
    k_msgq_peek(&mqtt.evtq, &event);

    if (event == MQTT_EVENT_CONNECTED) {
        return 0;
    }

    mqtt_abort(&mqtt.client);
    return ENOTCONN;
}

static void client_disconnect(void) {
    int ret = mqtt_disconnect(&mqtt.client, NULL);
    if (ret) {
        mqtt_abort(&mqtt.client);
    }
}

static void client_setup(const char *user_name, const char *password) {
#define C mqtt.client

    mqtt_client_init(&C);

    C.broker = &mqtt.broker;

    C.protocol_version = MQTT_VERSION_3_1_1;
    C.transport.type = MQTT_TRANSPORT_NON_SECURE;

    C.evt_cb = mqtt_evt_handler;

    C.client_id.utf8 = (uint8_t *)CLIENT_ID;
    C.client_id.size = sizeof(CLIENT_ID) - 1;

    C.password = NULL;
    if (password != NULL) {
        if (C.password == NULL)
            C.password = k_malloc(sizeof(struct mqtt_utf8));
        C.password->utf8 = (uint8_t *)password;
        C.password->size = strlen(password);
    }

    C.user_name = NULL;
    if (user_name != NULL) {
        if (C.user_name == NULL)
            C.user_name = k_malloc(sizeof(struct mqtt_utf8));
        C.user_name->utf8 = (uint8_t *)user_name;
        C.user_name->size = strlen(user_name);
    }

    C.rx_buf = (uint8_t *)&mqtt.rx_buffer;
    C.rx_buf_size = sizeof(mqtt.rx_buffer);

    C.tx_buf = (uint8_t *)&mqtt.tx_buffer;
    C.tx_buf_size = sizeof(mqtt.tx_buffer);

#undef C
}

static void client_broker_setup(const char *ip_addr, uint16_t port) {
    struct sockaddr_in *addr = (struct sockaddr_in *)&mqtt.broker;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    inet_pton(AF_INET, ip_addr, &addr->sin_addr);
}

void mqtt_init(
    const char *ip_addr,
    uint16_t port,
    const char *user_name,
    const char *password
) {
    if (mqtt.init) {
        client_disconnect();
    }

    client_broker_setup(ip_addr, port);
    client_setup(user_name, password);

    if (mqtt.init) return;

    setup_mqtt_evtq();
    start_mqtt_thread();

    LOG_WRN("Test 1");

    enum mqtt_event mqtt_event = MQTT_EVENT_SETUP_COMPLETE;
    k_msgq_put(&mqtt.evtq, &mqtt_event, K_FOREVER);

    LOG_WRN("Test 2");

    mqtt.init = true;
}

static void setup_mqtt_evtq(void) {
    k_msgq_init(
        &mqtt.evtq,
        (char*)&mqtt.evtq_buffer,
        sizeof(enum mqtt_event),
        sizeof(mqtt.evtq_buffer) / sizeof(enum mqtt_event)
    );
}

static void start_mqtt_thread(void) {
    if (mqtt.tid) return;

    mqtt.tid = k_thread_create(
        &mqtt_thread_data, mqtt_thread_stack_area,
        K_THREAD_STACK_SIZEOF(mqtt_thread_stack_area),
        mqtt_thread,
        NULL, NULL, NULL,
        mqtt_thread_priority, 0, K_NO_WAIT);
}

static void mqtt_thread(void *unused1, void *unused2, void *unused3) {
    int ret;
    enum mqtt_event event;

    for (;;) {
        mqtt_input(&mqtt.client);
        mqtt_live(&mqtt.client);

        ret = k_msgq_get(&mqtt.evtq, &event, K_NO_WAIT);
        if (ret) {
            k_yield();
            continue;
        }

        LOG_WRN("Received MQTT event: %d", event);

        switch (event) {
            case MQTT_EVENT_CONNECTED:
                LOG_INF("Successfully connected to MQTT!");
                break;

            case MQTT_EVENT_DISCONNECTED:
                LOG_WRN("MQTT is disconnected...");

            case MQTT_EVENT_CONNECTION_FAILED:
                k_sleep(RECONNECT_DELAY);

            case MQTT_EVENT_SETUP_COMPLETE:
                ret = client_connect();
                if (ret) {
                    LOG_WRN("Failed to send connection request: %d", ret);

                    const enum mqtt_event retry_event = MQTT_EVENT_CONNECTION_FAILED;
                    k_msgq_put(&mqtt.evtq, &retry_event, K_FOREVER);
                }
                break;

            default:
                break;
        }
    }
}

int mqtt_publish_msg(const char *topic, const char *payload, int qos) {
	struct mqtt_publish_param param = {0};

    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);

	param.message.topic.qos = qos;
	param.message.payload.data = (uint8_t *)payload;
	param.message.payload.len = strlen(payload);
	param.message_id = sys_rand16_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	return mqtt_publish(&mqtt.client, &param);
}
