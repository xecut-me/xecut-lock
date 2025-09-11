#include <stdint.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <stdio.h>
#include <string.h>

uint8_t kdf_key[] = {0x35, 0x0a , 0xb9 , 0x76 , 0x36 , 0x5a , 0x37 , 0xee  , 0x84 , 0x70 , 0xad , 0x8a , 0xe0 , 0xf7 , 0x0e , 0xe8};

int main(){
    uint8_t otp_key[0x30];
    char uid[16];
    fgets(uid,sizeof(uid),stdin);
    uint8_t uid_len  = strlen(uid); //ignore last symbol of uid as it is \n
    if (uid[uid_len-1]=='\n') uid_len--;
    mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1, uid, uid_len, kdf_key, sizeof(kdf_key), 4000, sizeof(otp_key), otp_key);
    for (int i=0;i<sizeof(otp_key);i++){
        printf("%02x",otp_key[i]);
    }
    //printf("\n");
    return 0;
}
