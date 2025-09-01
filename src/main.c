#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/clock.h>

#include "wifi.h"
#include "dns.h"
#include "sntp.h"

LOG_MODULE_REGISTER(MAIN);

#define WIFI_SSID "CGA2121_7d2gR7y"
#define WIFI_PSK  "N4fzQqD9Tsv7rjy6bp"

#define MY_STACK_SIZE 10240
#define MY_PRIORITY 5

extern void my_entry_point(void *, void *, void *);

K_THREAD_STACK_DEFINE(my_stack_area, MY_STACK_SIZE);
struct k_thread my_thread_data;

K_THREAD_STACK_DEFINE(my_stack_area2, MY_STACK_SIZE);
struct k_thread my_thread_data2;

void demo_request(void) {
#define HTTP_HOST "example.com"
#define HTTP_URL "/"

    char response[512];
    char http_request[512];
    int sock;
    int len;
    uint32_t rx_total;
    int ret;

    // Construct HTTP GET request
    snprintf(http_request,
             sizeof(http_request),
             "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n",
             HTTP_URL,
             HTTP_HOST);

    struct sockaddr addr;
    socklen_t addrlen;
    dns_query(HTTP_HOST, 80, AF_INET, SOCK_STREAM, &addr, &addrlen);

    // Create a new socket
    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printk("Error (%d): Could not create socket\r\n", errno);
        return;
    }

    // Connect the socket
    ret = zsock_connect(sock, &addr, addrlen);
    if (ret < 0) {
        printk("Error (%d): Could not connect the socket\r\n", errno);
        return;
    }

    // Set the request
    printk("Sending HTTP request...\r\n");
    ret = zsock_send(sock, http_request, strlen(http_request), 0);
    if (ret < 0) {
        printk("Error (%d): Could not send request\r\n", errno);
        return;
    }

    // Print the response
    printk("Response:\r\n\r\n");
    rx_total = 0;
    while (1) {

        // Receive data from the socket
        len = zsock_recv(sock, response, sizeof(response) - 1, 0);

        // Check for errors
        if (len < 0) {
            printk("Receive error (%d): %s\r\n", errno, strerror(errno));
            return;
        }

        // Check for end of data
        if (len == 0) {
            break;
        }

        // Null-terminate the response string and print it
        response[len] = '\0';
        printk("%s", response);
        rx_total += len;

        break;
    }

    // Print the total number of bytes received
    printk("\r\nTotal bytes received: %u\r\n", rx_total);

    // Close the socket
    zsock_close(sock);
}

void bumbum_thread(void *arg1, void *arg2, void *arg3) {
    for (;;) {
        demo_request();
        for (int i = 0; i < 120; i++) {
            LOG_INF("Time remaining: %d seconds", 120 - i);
            k_sleep(K_SECONDS(1));
        }
    }
}

void blink_thread(void *arg1, void *arg2, void *arg3) {
    #define LED0_NODE DT_ALIAS(led0)
    static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    for (;;) {
        gpio_pin_toggle_dt(&led);
        k_msleep(1000);
    }
}

int main(void) {
    int ret;

    wifi_init();
    
    ret = wifi_connect(WIFI_SSID, WIFI_PSK);
    if (ret) {
        LOG_ERR("Failed to connect to WiFi: %d", ret);
        return 0;
    }
    
    ret = wifi_get_status();
    if (ret) {
        LOG_ERR("Failed to get WiFi status: %d", ret);
        return 0;
    }

    // k_tid_t my_tid1 = k_thread_create(&my_thread_data, my_stack_area,
    //     K_THREAD_STACK_SIZEOF(my_stack_area),
    //     bumbum_thread,
    //     NULL, NULL, NULL,
    //     MY_PRIORITY, 0, K_NO_WAIT);

    // k_tid_t my_tid2 = k_thread_create(&my_thread_data2, my_stack_area2,
    //     K_THREAD_STACK_SIZEOF(my_stack_area),
    //     blink_thread,
    //     NULL, NULL, NULL,
    //     MY_PRIORITY, 0, K_NO_WAIT);

    // k_sleep(K_SECONDS(61 * 60));

    try_sync_time();

    for (;;) {
        struct timespec tp;
        int err = sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
        if (err) {
            LOG_ERR("Failed to get time");
            return 0;
        }

        uint32_t upt = k_uptime_seconds();

        printf("Uptime: %d, real time: %s\n", upt, ctime(&tp.tv_sec));

        k_msleep(1000);
    }

    return 0;
}
