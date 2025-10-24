#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct keypad_callbacks {
    bool (*command)(const char*);
    bool (*checkin)(const char*, const char*);
};

void keypad_init(struct keypad_callbacks cb);

void keypad_process(const char *data);
