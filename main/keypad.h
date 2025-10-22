#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct keypad_callbacks {
    bool (*command)(uint8_t*, size_t);
    bool (*checkin)(uint8_t*, size_t, uint8_t*, size_t);
};

void keypad_init(struct keypad_callbacks cb);

void keypad_process(const char *data);
