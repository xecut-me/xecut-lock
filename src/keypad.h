#pragma once

#include <stdbool.h>

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

typedef struct keypad keypad;

void keypad_init(
    const struct device *uart,
    struct keypad_callbacks cb,
    struct keypad *out
);

int keypad_poll(struct keypad *kp);
