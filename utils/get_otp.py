#!/usr/bin/env python3

import hashlib
import sys
import argparse
import base64

# These values must be identical to the values from the main/otp.c file.
KDF_ROUNDS   = 1000
OTP_KEY_SIZE = 0x30
OTP_DIGITS   = 6
OTP_TIMESTEP = 30

def validate_uid(uid: str):
    allowed_letters = ['P', 'T', 'M', 'B', 'O', 'S', 'L', 'A']
    allowed_letters_str = ', '.join(allowed_letters)
    max_len = 32

    if uid.startswith('P'):
        raise Exception(f"uid cannot start with the letter P")

    if len(uid) > max_len:
        raise Exception(f"uid too long, max length is {max_len} chars")

    for char in uid:
        if not char.isalnum() or (char.isalpha() and char not in allowed_letters):
            raise Exception(f"unsupported char '{char}', only 0-9 digit and {allowed_letters_str} letters are allowed")

def main():
    parser = argparse.ArgumentParser(description='Generate OTP key and otpauth URL')
    parser.add_argument('kdf_key_path', help='Path to KDF key file')
    parser.add_argument('uid', help='UID for OTP generation')

    args = parser.parse_args()

    try:
        validate_uid(args.uid)

        with open(args.kdf_key_path, 'rb') as f:
            kdf_key = f.read()

        uid_bytes = args.uid.encode('utf-8')

        otp_key = hashlib.pbkdf2_hmac(
            'sha1',
            uid_bytes,
            kdf_key,
            KDF_ROUNDS,
            OTP_KEY_SIZE,
        )

        otp_key_b32 = base64.b32encode(otp_key).decode('utf-8').rstrip('=')

        otpauth_url = f"otpauth://totp/{args.uid}?period={OTP_TIMESTEP}&digits={OTP_DIGITS}&algorithm=SHA1&secret={otp_key_b32}&issuer=Xecut"
        print(otpauth_url)

    except Exception as e:
        print(f"Failed to generate key: {e}", file=sys.stderr)
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
