#pragma once

#include <stdbool.h>

struct otp_key_info {
    char name[16];
    uint8_t key[128];
    uint8_t key_len;
    uint8_t digits;
    bool valid;
    uint32_t code;
    uint64_t step;
};
