#pragma once

#include <stdbool.h>
#include <stdint.h>


bool otp_verify_kdf(char *uid, size_t uid_len, char *code, size_t code_len);

int otp_init();
