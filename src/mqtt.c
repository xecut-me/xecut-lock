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

static struct sockaddr_storage broker;

static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

static struct mqtt_client client;
static struct pollfd fd;
static bool connected;

static void clear_fds(void) {
    fd.fd = -1;
}

static void mqtt_evt_handler(
    struct mqtt_client *client,
    const struct mqtt_evt *evt
) {
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result == 0) {
                connected = true;
            } else {
                LOG_WRN("MQTT connection failed with status code %d", evt->result);
            }
            break;
    
        case MQTT_EVT_DISCONNECT:
            LOG_WRN("MQTT client disconnected with status code %d", evt->result);
            connected = false;
            clear_fds();
            break;
        
        default:
            break;
    }
}

static void broker_init(
    const char *ip_addr,
    uint16_t port
) {
    struct sockaddr_in *addr = (struct sockaddr_in *)&broker;
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
    mqtt_client_init(&client);

    broker_init(ip_addr, port);

    client.broker = &broker;

    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;

    client.evt_cb = mqtt_evt_handler;

    client.client_id.utf8 = (uint8_t *)CLIENT_ID;
    client.client_id.size = sizeof(CLIENT_ID) - 1;

    client.password = NULL;
    if (password != NULL) {
        client.password = k_malloc(sizeof(struct mqtt_utf8));
        client.password->utf8 = (uint8_t *)password;
        client.password->size = strlen(password);
    }

    client.user_name = NULL;
    if (user_name != NULL) {
        client.user_name = k_malloc(sizeof(struct mqtt_utf8));
        client.user_name->utf8 = (uint8_t *)user_name;
        client.user_name->size = strlen(user_name);
    }

    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);

    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);
}

void prepare_fd(void) {
    fd.fd = client.transport.tcp.sock;
    fd.events = POLLIN;
}

void mqtt_run(void) {
    int ret;
    int try = 0;
    int max_try = 3;

    while (try++ < max_try) {
        ret = mqtt_connect(&client);
		if (ret != 0) {
			LOG_WRN("Failed to connect to MQTT broker: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
		}

        prepare_fd();

        ret = poll(&fd, 1, 2000);
        if (ret < 0) {
			LOG_WRN("Failed to poll MQTT broker: %d", ret);
			k_sleep(K_MSEC(1000));
			continue;
        }

        mqtt_input(&client);

        if (connected) {
            break;
        } else {
			mqtt_abort(&client);
		}
    }

    if (!connected) {
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
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	return mqtt_publish(&client, &param);
}

void mqtt_test(void) {
    int ret;

    ret = mqtt_ping(&client);
    LOG_INF("Ping result: %d", ret);

    char buffer[256] = {0};
    snprintf(&buffer, sizeof(buffer), "{\"value\": \"%d\"}", sys_rand32_get());

    ret = publish("xecut-lock/test/hop/hey/wtf", &buffer, MQTT_QOS_0_AT_MOST_ONCE);
    LOG_INF("Publish result: %d", ret);
}
