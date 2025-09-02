#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include <stdint.h>

#include "keypad.h"

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

enum keypad_state {
    KEYPAD_STATE_RESET = 0,
    KEYPAD_STATE_COMMAND = 1,
    KEYPAD_STATE_UID_INPUT = 2,
    KEYPAD_STATE_CODE_INPUT = 3,
};

static const char *keypad_state_txt(enum keypad_state state) {
    switch (state) {
        case KEYPAD_STATE_RESET:
            return "KEYPAD_STATE_RESET";
        case KEYPAD_STATE_COMMAND:
            return "KEYPAD_STATE_COMMAND";
        case KEYPAD_STATE_UID_INPUT:
            return "KEYPAD_STATE_UID_INPUT";
        case KEYPAD_STATE_CODE_INPUT:
            return "KEYPAD_STATE_CODE_INPUT";
        default:
            return "UNKNOWN";
    }
}

static int  keypad_handle_button(struct keypad *kp, uint8_t code);
static void keypad_reset(struct keypad *kp);
static int  keypad_handle_command(struct keypad *kp);
static void keypad_save_uid(struct keypad *kp);
static int  keypad_verify_code(struct keypad *kp);

struct keypad {
    // Internal keypad state. See img/keypad-state.jpg
    enum keypad_state state;

    // UART
    const struct device *device;

    // Callbacks for some final states
    struct keypad_callbacks callbacks;

    // Buffer for common state
    uint8_t *buffer;
    size_t buffer_len;

    // Special buffer for UID
    uint8_t *uid_buffer;
    size_t uid_buffer_len;
};

void keypad_init(
    const struct device *uart,
    struct keypad_callbacks cb,
    struct keypad *out
) {
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
}

int keypad_poll(struct keypad *kp) {
    uint8_t byte;

    if (kp->buffer_len == KEYPAD_MAX_BUFFER_SIZE) {
        return KEYPAD_STATUS_BUFFER_OVERFLOW;
    }

    if (uart_poll_in(kp->device, &byte)) {
        return KEYPAD_STATUS_EMPTY_UART;
    }

    return keypad_handle_button(kp, byte);
}

static int keypad_handle_button(struct keypad *kp, uint8_t code) {
    switch (code) {
        // Reset state
        case KEYPAD_BTN_CLEAR:
            keypad_reset(kp);
            break;

        // Special code
        case KEYPAD_BTN_POWER:
            if (kp->state == KEYPAD_STATE_RESET) {
                kp->state = KEYPAD_STATE_COMMAND;
            } else {
                return KEYPAD_STATUS_INVALID_STATE;
            }

            break;

        // Next state
        case KEYPAD_BTN_ENTER:
            if (kp->buffer_len == 0) {
                break;
            }

            if (kp->state == KEYPAD_STATE_COMMAND) {
                return keypad_handle_command(kp);
            }
            else if (kp->state == KEYPAD_STATE_UID_INPUT) {
                keypad_save_uid(kp);
                kp->state = KEYPAD_STATE_CODE_INPUT;
            }
            else if (kp->state == KEYPAD_STATE_CODE_INPUT) {
                return keypad_verify_code(kp);
            }
            else {
                return KEYPAD_STATUS_INVALID_STATE;
            }

            break;

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
            if (kp->state == KEYPAD_STATE_RESET) {
                kp->state = KEYPAD_STATE_UID_INPUT;
            }

            kp->buffer[kp->buffer_len++] = code;
            break;

        // Unused buttons
        case KEYPAD_BTN_TBL:
        case KEYPAD_BTN_MEM:
        case KEYPAD_BTN_BYP:
        case KEYPAD_BTN_OFF:
        case KEYPAD_BTN_STAY:
        case KEYPAD_BTN_SLEEP:
        case KEYPAD_BTN_ARM:
            break;

        // Someone connected via flipper and is trying to hack us!
        default:
            return KEYPAD_STATUS_UNKNOWN_BUTTON;
    }

    return KEYPAD_STATUS_OK;
}

static void keypad_reset(struct keypad *kp) {
    kp->state = KEYPAD_STATE_RESET;

    memset(kp->buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    kp->buffer_len = 0;

    memset(kp->uid_buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    kp->uid_buffer_len = 0;
}

static int keypad_handle_command(struct keypad *kp) {
    ASSERT_STATE(kp->state, KEYPAD_STATE_COMMAND);

    bool result = kp->callbacks.command(
        kp->buffer,
        kp->buffer_len
    );

    return result ? KEYPAD_STATUS_OK : KEYPAD_STATUS_BAD_CODE;
}

static void keypad_save_uid(struct keypad *kp) {
    ASSERT_STATE(kp->state, KEYPAD_STATE_UID_INPUT);

    memcpy(kp->uid_buffer, kp->buffer, kp->buffer_len);
    kp->uid_buffer_len = kp->buffer_len;
}

static int keypad_verify_code(struct keypad *kp) {
    ASSERT_STATE(kp->state, KEYPAD_STATE_CODE_INPUT);

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
