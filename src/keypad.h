#pragma once

#include <stdbool.h>

enum keypad_state {
    KEYPAD_STATE_RESET = 0,
    KEYPAD_STATE_COMMAND = 1,
    KEYPAD_STATE_UID_INPUT = 2,
    KEYPAD_STATE_CODE_INPUT = 3,
};

enum keypad_status {
    KEYPAD_STATUS_OK = 0,
    KEYPAD_STATUS_EMPTY_UART = -1,
    KEYPAD_STATUS_BUFFER_OVERFLOW = -2,
    KEYPAD_STATUS_INVALID_STATE = -3,
    KEYPAD_STATUS_UNHANDLED_COMMAND = -4,
    KEYPAD_STATUS_BAD_CODE = -5,
    KEYPAD_STATUS_UNKNOWN_BUTTON = -6,
};

struct keypad_callbacks {
    bool (*command)(uint8_t*, size_t);
    bool (*checkin)(uint8_t*, size_t, uint8_t*, size_t);
};

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
);

enum keypad_status keypad_poll(struct keypad *kp);

const char *keypad_state_txt(enum keypad_state state);

const char *keypad_status_txt(enum keypad_status status);