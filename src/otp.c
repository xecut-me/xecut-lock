#include <stdint.h>
#include <stdio.h>
#include <mbedtls/md.h>
#include <byteswap.h>
#include <math.h>
#include <zephyr/sys/util.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>

#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/timing/timing.h>
#include <stdlib.h>
#include <time.h>

#include "otp.h"

#define CONFIG_OTP_MAX_SAVED 10

struct otp_key_info otp_cache[CONFIG_OTP_MAX_SAVED];

size_t base32_decode(const char* input, uint8_t* output, size_t max_output) {
    uint32_t buffer = 0;
    int bits_left = 0;
    size_t out_len = 0;

    while (*input && out_len < max_output) {
        char c = toupper((unsigned char)*input++);
        uint8_t val;

        if (c >= 'A' && c <= 'Z') {
            val = c - 'A';
        } else if (c >= '2' && c <= '7') {
            val = c - '2' + 26;
        } else if (c == '=') {
            break;  // padding character
        } else {
            continue;  // skip invalid chars like spaces or dashes
        }

        buffer = (buffer << 5) | val;
        bits_left += 5;

        if (bits_left >= 8) {
            bits_left -= 8;
            output[out_len++] = (uint8_t)((buffer >> bits_left) & 0xFF);
        }
    }

    return out_len;
}

uint32_t otp_truncate(const uint8_t *digest, uint8_t digits){
	uint64_t offset;
	uint32_t bin_code;

	offset = digest[19] & 0x0f;
	bin_code = (digest[offset] & 0x7fu) << 24u
               | (digest[offset+1u] & 0xffu) << 16u
               | (digest[offset+2u] & 0xffu) << 8u
               | (digest[offset+3u] & 0xffu);

    return bin_code % (int)pow(10,digits);

};

uint32_t _get_otp(uint8_t key[], uint8_t key_len, uint8_t digits, uint64_t step){
	uint8_t digest[128];
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  #ifndef bswap_64
    #define bswap_64(x)			\
    ((((x) & 0xff00000000000000ull) >> 56)	\
    | (((x) & 0x00ff000000000000ull) >> 40)	\
    | (((x) & 0x0000ff0000000000ull) >> 24)	\
    | (((x) & 0x000000ff00000000ull) >> 8)	\
    | (((x) & 0x00000000ff000000ull) << 8)	\
    | (((x) & 0x0000000000ff0000ull) << 24)	\
    | (((x) & 0x000000000000ff00ull) << 40)	\
    | (((x) & 0x00000000000000ffull) << 56))
  #endif
  step=bswap_64(step);
  #endif

  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), key, key_len , (const unsigned char*)&step, sizeof(step), digest);
  return otp_truncate(digest,6);
};

static int otp_load_cb(const char* key, size_t len, settings_read_cb read_cb, void *cb_arg, void *param){
	struct otp_key_info *info = param;
	strcpy(info->name,key);
	info->key_len = len;
	read_cb(cb_arg,info->key,len);
	printf("OTP key loaded for %s: len %d\n",info->name,info->key_len);
	printf("%d %lld",info->digits,info->step);
	info->code = _get_otp(info->key,info->key_len,info->digits,info->step);
	return 0;
};

uint32_t get_otp(uint64_t step){
	struct otp_key_info info = {
		.digits=6,
		.step = step
	};
	settings_load_subtree_direct("otp",otp_load_cb,&info);
	return info.code ;//_get_otp(info.key,info.key_len,6,step);
}
static int otp_verify_cb(const char* key, size_t len, settings_read_cb read_cb, void *cb_arg, void *param){
	struct otp_key_info *info = param;
	if (key){
		strcpy(info->name,key);
	}
	info->key_len = len;
	read_cb(cb_arg,info->key,len);
	printf("OTP key loaded for %s: len %d\n",info->name,info->key_len);
	printf("%d %lld\n",info->digits,info->step);
	uint32_t otp = _get_otp(info->key,info->key_len,info->digits,info->step);
	info->valid = info->code == otp;
	printf("code for %s is %06d/%06d (%d)\n",info->name,info->code,otp,info->valid);
	return info->valid;
};

// static int cmd_verify(const struct shell *sh, size_t argc, char **argv)
// {
// 	shell_print(sh, "argc = %zd", argc);
// 	for (size_t cnt = 0; cnt < argc; cnt++) {
// 		shell_print(sh, "argv[%zd]", cnt);
// 		shell_hexdump(sh, argv[cnt], strlen(argv[cnt]));
// 	}
// 	timing_init();
// 	struct timespec tp;
// 	sys_clock_gettime(SYS_CLOCK_REALTIME,&tp);
// 	char path[128] = "otp";
// 	struct otp_key_info info = {
// 		.digits=6,
// 		.step = tp.tv_sec/30,
// 		.code = atoi(argv[1])
// 	};
// 	if (argc>2){
// 		snprintf(path,128,"otp/%s",argv[2]);
// 		strncpy(info.name,argv[2],16);
// 	}
// 	shell_print(sh, "path= %s", path);
// 	shell_print(sh, "current time: %s",ctime(&tp.tv_sec));
// 	timing_start();
// 	timing_t start,end;
// 	start = timing_counter_get();
// 	settings_load_subtree_direct(path,otp_verify_cb,&info);
// 	end = timing_counter_get();
// 	timing_stop();
// 	printf("time: %lld\n",timing_cycles_to_ns(timing_cycles_get(&start,&end)));
// 	shell_print(sh, "result= %d", info.valid);
// 	return 0;
// }
#include <mbedtls/pkcs5.h>
extern uint8_t kdf_key[16] ;


// bool otp_verify_kdf(char* uid, size_t uid_len, char* code, size_t code_len){
// 	struct timespec tp;
// 	sys_clock_gettime(SYS_CLOCK_REALTIME,&tp);
// 	struct otp_key_info info = {
// 		.digits=6,
// 		.step = tp.tv_sec/30,
// 		.code = atoi(code),
// 		.key_len = 0x30
// 	};
// 	strncpy(info.name,uid,uid_len<16? uid_len:16);

// 	timing_start();
// 	timing_t start,end;
// 	start = timing_counter_get();
// 	mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1,
// 								  uid,uid_len,
// 								  kdf_key, sizeof(kdf_key),
// 								  4000 ,
// 							   info.key_len, info.key);

// 	uint32_t otp = _get_otp(info.key,info.key_len,info.digits,info.step);
// 	info.valid = info.code == otp;
// 	printf("code for %s is %06d/%06d (%d)\n",info.name,info.code,otp,info.valid);
// 	end = timing_counter_get();
// 	timing_stop();
// 	printf("time: %lld\n",timing_cycles_to_ns(timing_cycles_get(&start,&end)));
// 	return info.valid;
// }

// static int cmd_verify_kdf(const struct shell *sh, size_t argc, char **argv)
// {

// 	struct timespec tp;
// 	sys_clock_gettime(SYS_CLOCK_REALTIME,&tp);
// 	struct otp_key_info info = {
// 		.digits=6,
// 		.step = tp.tv_sec/30,
// 		.code = atoi(argv[2]),
// 		.key_len = 0x30
// 	};
// 	strncpy(info.name,argv[1],16);

// 	timing_start();
// 	timing_t start,end;
// 	start = timing_counter_get();
// 	mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1,
// 								  argv[1],strlen(argv[1]),
// 								  kdf_key, sizeof(kdf_key),
// 								  4000 ,
// 								  info.key_len, info.key);

// 	uint32_t otp = _get_otp(info.key,info.key_len,info.digits,info.step);
// 	info.valid = info.code == otp;
// 	printf("code for %s is %06d/%06d (%d)\n",info.name,info.code,otp,info.valid);
// 	end = timing_counter_get();
// 	timing_stop();
// 	shell_hexdump(sh, info.key, info.key_len);
// 	printf("time: %lld\n",timing_cycles_to_ns(timing_cycles_get(&start,&end)));
// 	shell_print(sh, "result= %d", info.valid);
// 	return 0;
// }

// SHELL_CMD_ARG_REGISTER(verify, NULL, "Verify OTP code.\n verify code [name]", cmd_verify, 2, 1);

// SHELL_CMD_ARG_REGISTER(verify_kdf, NULL, "Verify OTP code.\n verify userid code ", cmd_verify_kdf, 3, 0);
