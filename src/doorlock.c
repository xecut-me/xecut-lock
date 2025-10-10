#include "doorlock.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(RELAY);

#ifdef CONFIG_REAL_RELAY
#define RELAY_NODE DT_CHOSEN(xecut_doorlock_relay)
static const struct gpio_dt_spec relay = GPIO_DT_SPEC_GET(RELAY_NODE, gpios);
#endif

int doorlock_init(void) {
#ifdef CONFIG_REAL_RELAY
    if (!gpio_is_ready_dt(&relay)) {
        return EAGAIN;
    }

   return gpio_pin_configure_dt(&relay, GPIO_OUTPUT_INACTIVE);
#else
    LOG_WRN("Real door lock is disabled");
    return 0;
#endif
}

void doorlock_open(void) {
    const unsigned int key = irq_lock();

#ifdef CONFIG_REAL_RELAY
    gpio_pin_set_dt(&relay, 1);
#else
    LOG_WRN("Simulation of opening a door lock");
#endif

    k_busy_wait(200000);

#ifdef CONFIG_REAL_RELAY
    gpio_pin_set_dt(&relay, 0);
#else
    LOG_WRN("Simulation of closing a door lock");
#endif

    irq_unlock(key);
}