#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include <stdint.h>

#include "keypad.h"


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(keypad, 4);

#define KEYPAD_MAX_BUFFER_SIZE  32

#define KEYPAD_BTN_POWER   'P'
#define KEYPAD_BTN_TBL     'T'
#define KEYPAD_BTN_MEM     'M'
#define KEYPAD_BTN_BYP     'B'
#define KEYPAD_BTN_CLEAR   'C'
#define KEYPAD_BTN_ENTER   'E'
#define KEYPAD_BTN_OFF     'O'
#define KEYPAD_BTN_STAY    'S'
#define KEYPAD_BTN_SLEEP   'L'
#define KEYPAD_BTN_ARM     'A'

#define ASSERT_STATE(state, required_state) \
    __ASSERT((state) == required_state, "Required state is " #required_state " but keypad is in state %s", keypad_state_txt(state));

static enum keypad_status keypad_handle_button(struct keypad *kp, uint8_t code);
static void keypad_reset(struct keypad *kp);
static enum keypad_status keypad_next_state(struct keypad *kp);
static enum keypad_status keypad_handle_command(struct keypad *kp);
static void keypad_save_uid(struct keypad *kp);
static enum keypad_status keypad_verify_code(struct keypad *kp);
static enum keypad_status keypad_save_digit(struct keypad *kp, uint8_t code);

static void uart_int_handler(const struct device *uart_dev, void *user_data);
#define KPD_STACK_SIZE 2048
#define KPD_PRIORITY 5


K_EVENT_DEFINE(kpd_event);
K_THREAD_STACK_DEFINE(keypad_thread_stack, KPD_STACK_SIZE);
struct k_thread my_thread_data;

k_tid_t kpd_tid=NULL;


void keypad_thread(void *user_data, void *p2, void *p3){
    struct keypad *kp = (struct keypad *)user_data;
    __ASSERT(kp != NULL, "kp can`t be NULL");
    for(;;){
        printf("kp_thread wait event \n");
        k_event_wait(&kpd_event,0x3 ,false ,K_FOREVER );
        uart_irq_rx_disable(kp->device);
        printf("kp_thread got event %d\n",kpd_event.events);
        switch (kp->state) {
            case KEYPAD_STATE_COMMAND:
                keypad_handle_command(kp);
                keypad_reset(kp);
                break;
            case KEYPAD_STATE_CODE_INPUT:
                keypad_verify_code(kp);
                keypad_reset(kp);
                break;
            default:
                break;
        }
        printf("kp_thread clr event\n");
        k_event_clear(&kpd_event,kpd_event.events );
        uart_irq_rx_enable(kp->device);

    }
}

void keypad_init(
    const struct device *uart,
    struct keypad_callbacks cb,
    struct keypad *out
) {
    __ASSERT(out != NULL, "out can`t be NULL");
    uint8_t *buffer = k_malloc(KEYPAD_MAX_BUFFER_SIZE);
    __ASSERT(buffer != NULL, "Not enough memory for keypad buffer");

    uint8_t *uid_buffer = k_malloc(KEYPAD_MAX_BUFFER_SIZE);
    __ASSERT(uid_buffer != NULL, "Not enough memory for keypad uid buffer");

    *out = (struct keypad) {
        .state = KEYPAD_STATE_RESET,
        .device = uart,
        .callbacks = cb,
        .buffer = buffer,
        .buffer_len = 0,
        .uid_buffer = uid_buffer,
        .uid_buffer_len = 0,
    };


    uart_irq_callback_user_data_set(uart, uart_int_handler, (void *)out);
    uart_irq_rx_enable(uart);
    kpd_tid = k_thread_create(&my_thread_data, keypad_thread_stack,
                        K_THREAD_STACK_SIZEOF(keypad_thread_stack),
                        keypad_thread,
                        out, NULL, NULL,
                        KPD_PRIORITY, 0, K_NO_WAIT);
}

static void uart_int_handler(const struct device *uart_dev, void *user_data)
{
    uart_irq_update(uart_dev);
    struct keypad *kp = (struct keypad *)user_data;
    __ASSERT(kp != NULL, "kp can`t be NULL");
    if (uart_irq_rx_ready(uart_dev))
    {
        uint8_t byte;
        while (!uart_poll_in(uart_dev, &byte))
        {
            enum keypad_status status = keypad_handle_button(kp, byte);
            LOG_DBG("keypad_int byte %c status %s, current state is %s\n",byte, keypad_status_txt(status), keypad_state_txt(kp->state));
        }
    }
}

enum keypad_status keypad_poll(struct keypad *kp) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    uint8_t byte;

    if (uart_poll_in(kp->device, &byte)) {
        return KEYPAD_STATUS_EMPTY_UART;
    }

    return keypad_handle_button(kp, byte);
}

static enum keypad_status keypad_handle_button(struct keypad *kp, uint8_t code) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    switch (code) {
        // Reset state
        case KEYPAD_BTN_CLEAR:
            keypad_reset(kp);
            return KEYPAD_STATUS_OK;

        // Special code
        case KEYPAD_BTN_POWER:
            if (kp->state == KEYPAD_STATE_RESET) {
                kp->state = KEYPAD_STATE_COMMAND;
                return KEYPAD_STATUS_OK;
            } else {
                return KEYPAD_STATUS_INVALID_STATE;
            }

        // Next state
        case KEYPAD_BTN_ENTER:
            return keypad_next_state(kp);

        // Some numbers
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
            return keypad_save_digit(kp, code);

        // Unused buttons
        case KEYPAD_BTN_TBL:
        case KEYPAD_BTN_MEM:
        case KEYPAD_BTN_BYP:
        case KEYPAD_BTN_OFF:
        case KEYPAD_BTN_STAY:
        case KEYPAD_BTN_SLEEP:
        case KEYPAD_BTN_ARM:
            if (kp->state != KEYPAD_STATE_CODE_INPUT){
                return  keypad_save_digit(kp, code);
            }
            return KEYPAD_STATUS_OK;

        // Someone connected via flipper and is trying to hack us!
        default:
            return KEYPAD_STATUS_UNKNOWN_BUTTON;
    }

    return KEYPAD_STATUS_OK;
}

static void keypad_reset(struct keypad *kp) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    kp->state = KEYPAD_STATE_RESET;

    memset(kp->buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    kp->buffer_len = 0;

    memset(kp->uid_buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    kp->uid_buffer_len = 0;
}

static enum keypad_status keypad_next_state(struct keypad *kp) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    enum keypad_status status = KEYPAD_STATUS_INVALID_STATE;

    if (kp->buffer_len == 0) {
        return status;
    }

    if (kp->state == KEYPAD_STATE_COMMAND) {
        k_event_set(&kpd_event,1 );
        // status = keypad_handle_command(kp);
        // keypad_reset(kp);
        status = KEYPAD_STATUS_OK;
    }
    else if (kp->state == KEYPAD_STATE_UID_INPUT) {
        keypad_save_uid(kp);
        kp->state = KEYPAD_STATE_CODE_INPUT;

        status = KEYPAD_STATUS_OK;
    }
    else if (kp->state == KEYPAD_STATE_CODE_INPUT) {
        k_event_set(&kpd_event,2 );
        // status = keypad_verify_code(kp);
        // keypad_reset(kp);
        status = KEYPAD_STATUS_OK;
    }

    return status;
}

static enum keypad_status keypad_handle_command(struct keypad *kp) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    ASSERT_STATE(kp->state, KEYPAD_STATE_COMMAND);

    if (!kp->callbacks.command){
        LOG_DBG("callback command is empty");
        return KEYPAD_STATUS_UNHANDLED_COMMAND;
    }
    bool result = kp->callbacks.command(
        kp->buffer,
        kp->buffer_len
    );

    return result ? KEYPAD_STATUS_OK : KEYPAD_STATUS_BAD_CODE;
}

static void keypad_save_uid(struct keypad *kp) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    ASSERT_STATE(kp->state, KEYPAD_STATE_UID_INPUT);

    memcpy(kp->uid_buffer, kp->buffer, kp->buffer_len);
    kp->uid_buffer_len = kp->buffer_len;

    memset(kp->buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    kp->buffer_len = 0;
}

static enum keypad_status keypad_verify_code(struct keypad *kp) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    ASSERT_STATE(kp->state, KEYPAD_STATE_CODE_INPUT);
    if (!kp->callbacks.checkin){
        LOG_DBG("callback checkin is empty");
        return KEYPAD_STATUS_UNHANDLED_COMMAND;
    }
    bool result = kp->callbacks.checkin(
        // UID
        kp->uid_buffer,
        kp->uid_buffer_len,
        // Code
        kp->buffer,
        kp->buffer_len
    );

    return result ? KEYPAD_STATUS_OK : KEYPAD_STATUS_BAD_CODE;
}

static enum keypad_status keypad_save_digit(struct keypad *kp, uint8_t code) {
    __ASSERT(kp != NULL, "kp can`t be NULL");
    if (kp->state == KEYPAD_STATE_RESET) {
        kp->state = KEYPAD_STATE_UID_INPUT;
    }

    if (kp->buffer_len == KEYPAD_MAX_BUFFER_SIZE) {
        return KEYPAD_STATUS_BUFFER_OVERFLOW;
    }

    kp->buffer[kp->buffer_len++] = code;

    return KEYPAD_STATUS_OK;
}

const char *keypad_state_txt(enum keypad_state state) {
    switch (state) {
        case KEYPAD_STATE_RESET:
            return "RESET";
        case KEYPAD_STATE_COMMAND:
            return "COMMAND";
        case KEYPAD_STATE_UID_INPUT:
            return "UID_INPUT";
        case KEYPAD_STATE_CODE_INPUT:
            return "CODE_INPUT";
        default:
            return "UNKNOWN";
    }
}

const char *keypad_status_txt(enum keypad_status status) {
    switch (status) {
        case KEYPAD_STATUS_OK:
            return "OK";
        case KEYPAD_STATUS_EMPTY_UART:
            return "EMPTY_UART";
        case KEYPAD_STATUS_BUFFER_OVERFLOW:
            return "BUFFER_OVERFLOW";
        case KEYPAD_STATUS_INVALID_STATE:
            return "KEYPAD_STATUS_INVALID_STATE";
        case KEYPAD_STATUS_UNHANDLED_COMMAND:
            return "UNHANDLED_COMMAND";
        case KEYPAD_STATUS_BAD_CODE:
            return "BAD_CODE";
        case KEYPAD_STATUS_UNKNOWN_BUTTON:
            return "UNKNOWN_BUTTON";
        default:
            return "UNKNOWN";
    }
}
