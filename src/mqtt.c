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

static void client_setup(const char *user_name, const char *password);
static void client_broker_setup(const char *ip_addr, uint16_t port);
static void client_connected(void);
static void client_disconnected(void);

static void mqtt_evt_handler(
    struct mqtt_client *client,
    const struct mqtt_evt *evt
);

struct mqtt_manager {
    struct sockaddr_storage broker;

    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];
    struct pollfd fd;

    struct mqtt_client client;
    bool connected;
};

static struct mqtt_manager client = {0};

static void client_setup(const char *user_name, const char *password) {
#define C client.client

    mqtt_client_init(&C);

    C.broker = &client.broker;

    C.protocol_version = MQTT_VERSION_3_1_1;
    C.transport.type = MQTT_TRANSPORT_NON_SECURE;

    C.evt_cb = mqtt_evt_handler;

    C.client_id.utf8 = (uint8_t *)CLIENT_ID;
    C.client_id.size = sizeof(CLIENT_ID) - 1;

    C.password = NULL;
    if (password != NULL) {
        C.password = k_malloc(sizeof(struct mqtt_utf8));
        C.password->utf8 = (uint8_t *)password;
        C.password->size = strlen(password);
    }

    C.user_name = NULL;
    if (user_name != NULL) {
        C.user_name = k_malloc(sizeof(struct mqtt_utf8));
        C.user_name->utf8 = (uint8_t *)user_name;
        C.user_name->size = strlen(user_name);
    }

    C.rx_buf = (uint8_t *)&client.rx_buffer;
    C.rx_buf_size = sizeof(client.rx_buffer);

    C.tx_buf = (uint8_t *)&client.tx_buffer;
    C.tx_buf_size = sizeof(client.tx_buffer);

#undef C
}

static void client_broker_setup(const char *ip_addr, uint16_t port) {
    struct sockaddr_in *addr = (struct sockaddr_in *)&client.broker;
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	inet_pton(AF_INET, ip_addr, &addr->sin_addr);
}

static void client_connected(void) {
    client.connected = true;
}

static void client_disconnected(void) {
    client.connected = false;
    client.fd.fd = -1;
    client.fd.events = 0;
}

static void mqtt_evt_handler(
    struct mqtt_client *client,
    const struct mqtt_evt *evt
) {
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result == 0) {
                client_connected();
            } else {
                LOG_WRN("MQTT connection failed with status code %d", evt->result);
            }
            break;
    
        case MQTT_EVT_DISCONNECT:
            LOG_WRN("MQTT client disconnected with status code %d", evt->result);
            client_disconnected();
            break;

        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                LOG_ERR("MQTT PUBREC error %d", evt->result);
                break;
            }

            const struct mqtt_pubrel_param rel_param = {
                .message_id = evt->param.pubrec.message_id
            };

            int ret = mqtt_publish_qos2_release(client, &rel_param);
            if (ret != 0) {
                LOG_ERR("mqtt_publish_qos2_release failed with status code %d", ret);
            }

            break;

        default:
            break;
    }
}

void mqtt_init(
    const char *ip_addr,
    uint16_t port,
    const char *user_name,
    const char *password
) {
    client_broker_setup(ip_addr, port);
    client_setup(user_name, password);
}

void prepare_fd(void) {
    client.fd.fd = client.client.transport.tcp.sock;
    client.fd.events = POLLIN;
}

void mqtt_run(void) {
    int ret;
    int try = 0;
    int max_try = 3;

    while (try++ < max_try) {
        ret = mqtt_connect(&client.client);
		if (ret != 0) {
			LOG_WRN("Failed to connect to MQTT broker: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
		}

        prepare_fd();

        ret = poll(&client.fd, 1, 2000);
        if (ret < 0) {
			LOG_WRN("Failed to poll MQTT broker: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
        }

        mqtt_input(&client.client);

        if (client.connected) {
            break;
        } else {
			mqtt_abort(&client.client);
		}
    }

    if (!client.connected) {
        LOG_ERR("Meh(((");
    }
}

static int publish(const char *topic, const char *payload, enum mqtt_qos qos) {
	struct mqtt_publish_param param = {0};

    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);

	param.message.topic.qos = qos;
	param.message.payload.data = (uint8_t *)payload;
	param.message.payload.len = strlen(payload);
	param.message_id = sys_rand16_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	return mqtt_publish(&client.client, &param);
}

void mqtt_test(void) {
    int ret;

    ret = mqtt_ping(&client.client);
    LOG_INF("Ping result: %d", ret);

    int qos = 2;

    char buffer[256] = {0};
    snprintf((char*)&buffer, sizeof(buffer), "{\"value\": \"%d\", \"qos\": \"%d\"}", sys_rand32_get(), qos);

    ret = publish("xecut-lock/test/hop/hey/wtf", (char *)&buffer, qos);
    LOG_INF("Publish result: %d", ret);

    // Temporary hack to fix message sending with QOS 2.
    // Must be in separate thread
    for (int i = 0; i < 30; i++) {
        mqtt_input(&client.client);
        mqtt_live(&client.client);
        k_sleep(K_MSEC(100));
    }
}
