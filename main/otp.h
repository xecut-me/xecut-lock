#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

bool otp_verify(
    const uint8_t *uid,  size_t uid_len,
    const uint8_t *code, size_t code_len
);
