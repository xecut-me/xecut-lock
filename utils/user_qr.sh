uid=$1;
otp_secret=$(echo ${uid} | ./userkdf| xxd -ps -r | base32 | sed 's|=|%3D|g')
echo "otpauth://totp/Xecut%3A${uid}?period=30&digits=6&algorithm=SHA1&secret=${otp_secret}&issuer=Xecut" | tr -d '\n' | qrencode -t UTF8
