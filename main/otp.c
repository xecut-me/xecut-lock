#include "otp.h"

#include <esp_log.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <machine/endian.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#define TAG "otp"

#define CONFIG_KDF_KEY_NAME "kdf_secret"
#define CONFIG_KDF_KEY_MAX_SIZE 16
#define CONFIG_KDF_ROUNDS 4000
#define CONFIG_OTP_KEY_SIZE 0x30
#define CONFIG_OTP_DIGITS 6
#define CONFIG_OTP_TIMESTEP 30

uint8_t kdf_key[CONFIG_KDF_KEY_MAX_SIZE];
size_t kdf_key_size = 0;

static uint32_t otp_truncate(const uint8_t *digest, uint8_t digits) {
  uint64_t offset = digest[19] & 0x0f;
  uint32_t bin_code =
    (digest[offset + 0u] & 0x7fu) << 24u |
    (digest[offset + 1u] & 0xffu) << 16u |
    (digest[offset + 2u] & 0xffu) << 8u  |
    (digest[offset + 3u] & 0xffu);

  return bin_code % (int)pow(10, digits);
};

static uint32_t get_otp(
    uint8_t key[],
    uint8_t key_len,
    uint8_t digits,
    uint64_t step
) {
  uint8_t digest[128];

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#ifndef bswap_64
#define bswap_64(x)                          \
   ((((x) & 0xff00000000000000ull) >> 56) |  \
    (((x) & 0x00ff000000000000ull) >> 40) |  \
    (((x) & 0x0000ff0000000000ull) >> 24) |  \
    (((x) & 0x000000ff00000000ull) >> 8)  |  \
    (((x) & 0x00000000ff000000ull) << 8)  |  \
    (((x) & 0x0000000000ff0000ull) << 24) |  \
    (((x) & 0x000000000000ff00ull) << 40) |  \
    (((x) & 0x00000000000000ffull) << 56))
#endif
    step = bswap_64(step);
#endif

    mbedtls_md_hmac(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA1),
        key, key_len,
        (const unsigned char *)&step, sizeof(step),
        digest
    );
    
    return otp_truncate(digest, CONFIG_OTP_DIGITS);
};

int otp_verify(const char *uid, size_t uid_len, const char *code, size_t code_len) {
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t step = tv_now.tv_sec / CONFIG_OTP_TIMESTEP;

    uint8_t otp_key[CONFIG_OTP_KEY_SIZE];
    mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA1,
        (const unsigned char *)uid, uid_len,
        kdf_key, kdf_key_size,
        CONFIG_KDF_ROUNDS,
        sizeof(otp_key), otp_key
    );

    uint32_t user_code = atoi(code);
    uint32_t valid_code = get_otp(otp_key, sizeof(otp_key), CONFIG_OTP_DIGITS, step);
    int is_valid = user_code == valid_code;

    ESP_LOGI(
        TAG, "Code for user '%s' for step %llu is %06d, input is %06d, otp is %s\n",
        uid, step, valid_code, user_code, is_valid ? "valid" : "invalid"
    );

    return is_valid;
}
