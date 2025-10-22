#include "keypad.h"

#include <stdint.h>

#include <driver/uart.h>

#include "hardware.h"

#define TAG "keypad"

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

#define KEYPAD_MAX_BUFFER_SIZE  32

#define ASSERT_STATE(state, required_state) \
    if ((state) != required_state) { \
        ESP_LOGE(TAG, "Required state is " #required_state " but keypad is in state %s", keypad_state_str(state)); \
        abort(); \
    }

enum keypad_state {
    KEYPAD_STATE_RESET = 0,
    KEYPAD_STATE_COMMAND = 1,
    KEYPAD_STATE_UID_INPUT = 2,
    KEYPAD_STATE_CODE_INPUT = 3,
};

const char *keypad_state_str(enum keypad_state state) {
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

enum keypad_status {
    KEYPAD_STATUS_OK = 0,
    KEYPAD_STATUS_EMPTY_UART = -1,
    KEYPAD_STATUS_BUFFER_OVERFLOW = -2,
    KEYPAD_STATUS_INVALID_STATE = -3,
    KEYPAD_STATUS_UNHANDLED_COMMAND = -4,
    KEYPAD_STATUS_BAD_CODE = -5,
    KEYPAD_STATUS_UNKNOWN_BUTTON = -6,
};

const char *keypad_status_str(enum keypad_status status) {
    switch (status) {
        case KEYPAD_STATUS_OK:
            return "Success";
        case KEYPAD_STATUS_EMPTY_UART:
            return "Empty UART buffer";
        case KEYPAD_STATUS_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case KEYPAD_STATUS_INVALID_STATE:
            return "Invalid keypad state";
        case KEYPAD_STATUS_UNHANDLED_COMMAND:
            return "Unhandled command";
        case KEYPAD_STATUS_BAD_CODE:
            return "Invalid code entered";
        case KEYPAD_STATUS_UNKNOWN_BUTTON:
            return "Unknown button pressed";
        default:
            return "Unknown status";
    }
}

static struct {
    // Internal keypad state. See img/keypad-state.jpg
    enum keypad_state state;

    // Callbacks for some final states
    struct keypad_callbacks callbacks;

    // Buffer for common state
    uint8_t *buffer;
    size_t buffer_len;

    // Special buffer for UID
    uint8_t *uid_buffer;
    size_t uid_buffer_len;
} keypad = {0};

static enum keypad_status keypad_save_digit(char chr) {
    if (keypad.state == KEYPAD_STATE_RESET) {
        keypad.state = KEYPAD_STATE_UID_INPUT;
    }

    if (keypad.buffer_len == KEYPAD_MAX_BUFFER_SIZE) {
        return KEYPAD_STATUS_BUFFER_OVERFLOW;
    }

    keypad.buffer[keypad.buffer_len++] = chr;

    return KEYPAD_STATUS_OK;
}

static void keypad_save_uid(void) {
    ASSERT_STATE(keypad.state, KEYPAD_STATE_UID_INPUT);

    memcpy(keypad.uid_buffer, keypad.buffer, keypad.buffer_len);
    keypad.uid_buffer_len = keypad.buffer_len;

    memset(keypad.buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    keypad.buffer_len = 0;
}

static enum keypad_status keypad_handle_command(void) {
    ASSERT_STATE(keypad.state, KEYPAD_STATE_COMMAND);

    bool result = keypad.callbacks.command(
        keypad.buffer,
        keypad.buffer_len
    );

    return result ? KEYPAD_STATUS_OK : KEYPAD_STATUS_BAD_CODE;
}

static enum keypad_status keypad_verify_code(void) {
    ASSERT_STATE(keypad.state, KEYPAD_STATE_CODE_INPUT);

    bool result = keypad.callbacks.checkin(
        // UID
        keypad.uid_buffer,
        keypad.uid_buffer_len,
        // Code
        keypad.buffer,
        keypad.buffer_len
    );

    return result ? KEYPAD_STATUS_OK : KEYPAD_STATUS_BAD_CODE;
}

static void keypad_reset(void) {
    keypad.state = KEYPAD_STATE_RESET;

    memset(keypad.buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    keypad.buffer_len = 0;

    memset(keypad.uid_buffer, 0, KEYPAD_MAX_BUFFER_SIZE);
    keypad.uid_buffer_len = 0;
}

static enum keypad_status keypad_next_state(void) {
    enum keypad_status status = KEYPAD_STATUS_INVALID_STATE;

    if (keypad.buffer_len == 0) {
        return status;
    }

    if (keypad.state == KEYPAD_STATE_COMMAND) {
        status = keypad_handle_command();
        keypad_reset();
    }
    else if (keypad.state == KEYPAD_STATE_UID_INPUT) {
        keypad_save_uid();
        keypad.state = KEYPAD_STATE_CODE_INPUT;

        status = KEYPAD_STATUS_OK;
    }
    else if (keypad.state == KEYPAD_STATE_CODE_INPUT) {
        status = keypad_verify_code();
        keypad_reset();
    }

    return status;
}

static enum keypad_status keypad_handle_button(char chr) {
    switch (chr) {
        // Reset state
        case KEYPAD_BTN_CLEAR:
            keypad_reset();
            return KEYPAD_STATUS_OK;

        // Special code
        case KEYPAD_BTN_POWER:
            if (keypad.state == KEYPAD_STATE_RESET) {
                keypad.state = KEYPAD_STATE_COMMAND;
                return KEYPAD_STATUS_OK;
            } else {
                return KEYPAD_STATUS_INVALID_STATE;
            }

        // Next state
        case KEYPAD_BTN_ENTER:
            return keypad_next_state();

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
            return keypad_save_digit(chr);

        // Unused buttons
        case KEYPAD_BTN_TBL:
        case KEYPAD_BTN_MEM:
        case KEYPAD_BTN_BYP:
        case KEYPAD_BTN_OFF:
        case KEYPAD_BTN_STAY:
        case KEYPAD_BTN_SLEEP:
        case KEYPAD_BTN_ARM:
            return KEYPAD_STATUS_OK;

        // Someone connected via flipper and is trying to hack us!
        default:
            return KEYPAD_STATUS_UNKNOWN_BUTTON;
    }

    return KEYPAD_STATUS_OK;
}

void keypad_init(struct keypad_callbacks cb) {
    keypad.state = KEYPAD_STATE_RESET;

    keypad.buffer = malloc(KEYPAD_MAX_BUFFER_SIZE);
    assert(keypad.buffer != NULL);

    keypad.uid_buffer = malloc(KEYPAD_MAX_BUFFER_SIZE);
    assert(keypad.uid_buffer != NULL);

    keypad.callbacks = cb;
}

void keypad_process(const char *data) {
    ESP_LOGI(TAG, "Process input '%s'", data);

    while (*data++ != '\0') {
        char chr = *data;
        
        enum keypad_status status = keypad_handle_button(chr);    
        if (status != KEYPAD_STATUS_OK) {
            ESP_LOGW(TAG, "Failed to process input '%c': %s", chr, keypad_status_str(status));
        }
    }
}
