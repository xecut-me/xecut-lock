#include <byteswap.h>
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <zephyr/sys/util.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(otp, 4);

#ifdef CONFIG_TIMING_FUNCTIONS
#include <zephyr/timing/timing.h>
#endif

#include "otp.h"

#define CONFIG_KDF_KEY_NAME "kdf_secret"
#define CONFIG_KDF_KEY_MAX_SIZE 16
#define CONFIG_OTP_KEY_SIZE 0x30
#define CONFIG_OTP_DIGITS 6
#define CONFIG_OTP_TIMESTEP 30

uint32_t otp_truncate(const uint8_t *digest, uint8_t digits) {
  uint64_t offset;
  uint32_t bin_code;

  offset = digest[19] & 0x0f;
  bin_code =
      (digest[offset] & 0x7fu) << 24u | (digest[offset + 1u] & 0xffu) << 16u |
      (digest[offset + 2u] & 0xffu) << 8u | (digest[offset + 3u] & 0xffu);

  return bin_code % (int)pow(10, digits);
};

uint32_t _get_otp(uint8_t key[], uint8_t key_len, uint8_t digits,
                  uint64_t step) {
  uint8_t digest[128];
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#ifndef bswap_64
#define bswap_64(x)                                                            \
  ((((x) & 0xff00000000000000ull) >> 56) |                                     \
   (((x) & 0x00ff000000000000ull) >> 40) |                                     \
   (((x) & 0x0000ff0000000000ull) >> 24) |                                     \
   (((x) & 0x000000ff00000000ull) >> 8) |                                      \
   (((x) & 0x00000000ff000000ull) << 8) |                                      \
   (((x) & 0x0000000000ff0000ull) << 24) |                                     \
   (((x) & 0x000000000000ff00ull) << 40) |                                     \
   (((x) & 0x00000000000000ffull) << 56))
#endif
  step = bswap_64(step);
#endif

  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), key, key_len,
                  (const unsigned char *)&step, sizeof(step), digest);
  return otp_truncate(digest, 6);
};



uint8_t kdf_key[CONFIG_KDF_KEY_MAX_SIZE];
size_t kdf_key_size = 0;

bool otp_verify_kdf(char *uid, size_t uid_len, char *code, size_t code_len) {
  struct timespec tp;
  uint8_t otp_key[0x30];
  sys_clock_gettime(SYS_CLOCK_REALTIME, &tp);
  uint32_t i_code = atoi(code);
  LOG_WRN("checking otp: uid=%s code=%06d\n",uid,i_code);
#ifdef CONFIG_TIMING_FUNCTIONS
  timing_start();
  timing_t start, end;
  start = timing_counter_get();
#endif
  mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1, uid, uid_len, kdf_key,
                                kdf_key_size, 4000, sizeof(otp_key), otp_key);

  uint32_t otp = _get_otp(otp_key, sizeof(otp_key), CONFIG_OTP_DIGITS, (CONFIG_OTP_TIMESTEP / 30));
  bool valid = otp == i_code;
  LOG_DBG("code for %s is %06d/%06d (%d)\n", uid, i_code, otp, valid);
#ifdef CONFIG_TIMING_FUNCTIONS
  end = timing_counter_get();
  timing_stop();
  LOG_DBG("time: %lld\n", timing_cycles_to_ns(timing_cycles_get(&start, &end)));
#endif
  return valid;
}

int otp_init() {
  int ret;
  ret = settings_subsys_init();
  if (ret) {
    LOG_DBG("settings subsys initialization: fail (err %d)\n", ret);
    return ret;
  }
  LOG_DBG("settings subsys initialization: OK.\n");
  ssize_t kdf_key_size =
      settings_load_one(CONFIG_KDF_KEY_NAME, kdf_key, sizeof(kdf_key));
  if (kdf_key_size != CONFIG_KDF_KEY_MAX_SIZE) {
        LOG_WRN("Saved KDF key is less than configured %ld/%d",kdf_key_size,CONFIG_KDF_KEY_MAX_SIZE);
    }
    return 0;
}
