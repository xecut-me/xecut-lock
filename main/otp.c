#include "otp.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <machine/endian.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#define TAG "otp"

#define KDF_ROUNDS 1000
#define OTP_KEY_SIZE 0x30
#define OTP_DIGITS 6
#define OTP_TIMESTEP 30

// Uncomment this line to get timings of otp_verify.
// #define DEBUG_PERFORMANCE

const uint8_t kdf_key[] = {
    #embed "../private/key.bin"
};

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
    
    return otp_truncate(digest, OTP_DIGITS);
};

uint32_t str_to_uint32(const char *str) {
    uint32_t ret = 0;
    for (char chr = *str; chr != '\0'; chr = *++str) {
        ret = ret * 10 + (chr - '0');
    }
    return ret;
}

bool otp_verify(const char *uid, const char *code) {
#ifdef DEBUG_PERFORMANCE
    uint64_t start = esp_timer_get_time();
#endif

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t step = tv_now.tv_sec / OTP_TIMESTEP;

    uint8_t otp_key[OTP_KEY_SIZE];
    mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA1,
        (const uint8_t*)uid, strlen(uid),
        kdf_key, sizeof(kdf_key),
        KDF_ROUNDS,
        sizeof(otp_key), otp_key
    );

    uint32_t user_code = str_to_uint32(code);
    uint32_t valid_code = get_otp(otp_key, sizeof(otp_key), OTP_DIGITS, step);
    int is_valid = user_code == valid_code;

#ifdef DEBUG_PERFORMANCE
    uint64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "otp_verify took %llu milliseconds to get otp", (end - start)/1000);
#endif

    ESP_LOGD(
        TAG, "Code for user '%s' for step %llu is %06d, input is %06d, otp is %s",
        uid, step, valid_code, user_code, is_valid ? "valid" : "invalid"
    );

    return is_valid;
}
