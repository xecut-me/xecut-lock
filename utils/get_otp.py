#!/usr/bin/env python3

import hashlib
import sys
import argparse
import base64

# The values must be identical to the values from main/otp.c.
CONFIG_KDF_ROUNDS = 4000
CONFIG_OTP_KEY_SIZE = 0x30

def main():
    parser = argparse.ArgumentParser(description='Generate OTP key and otpauth URL')
    parser.add_argument('kdf_key_path', help='Path to KDF key file')
    parser.add_argument('uid', help='UID for OTP generation')

    args = parser.parse_args()

    try:
        with open(args.kdf_key_path, 'rb') as f:
            kdf_key = f.read()

        uid_bytes = args.uid.encode('utf-8')

        otp_key = hashlib.pbkdf2_hmac(
            'sha1',
            uid_bytes,
            kdf_key,
            CONFIG_KDF_ROUNDS,
            CONFIG_OTP_KEY_SIZE,
        )

        otp_key_b32 = base64.b32encode(otp_key).decode('utf-8').rstrip('=')

        otpauth_url = f"otpauth://totp/{args.uid}?period=30&digits=6&algorithm=SHA1&secret={otp_key_b32}&issuer=Xecut"
        print(otpauth_url)

    except Exception as e:
        print(f"Failed to generate key: {e}", file=sys.stderr)
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
