# Xecut Lock

A smart lock project with OTP codes, MQTT control, and WiFi/Ethernet connectivity. Built on the ESP32-S3 microcontroller, but should work on other Espressif microcontrollers as well.

## Preparing to Build the Firmware

First, clone the project and then create `main/config.h` and generate kdf key.

### Creating config.h

The `main/config.h` file specifies WiFi login and password, MQTT credentials, internet connection method selection and lock identifier. This file contains sensitive data and should never be committed.

A config template is located in the [main/config.template.h](main/config.template.h) file. Copy it to `main/config.h`, edit it for your needs, and you can proceed to the next step.

### KDF Key Generation

The lock doesn't store OTP keys but generates them on-the-fly for specific users from a KDF key. The key itself is a random set of bytes of arbitrary length.

The key should be located in the `private/key.bin` directory.

To generate a key, run the script:

```bash
./utils/gen_kdf.py ./private/key.bin 48
```

Where `48` is the key length.

## Building the Firmware

To build the project, you'll need ESP-IDF version 6.0 or higher installed and configured. If you don't have this SDK, you can read about installation and toolchain setup here: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html.

After that, you can run `idf.py build` and `idf.py flash`. No special rituals required.

## Connecting the Keypad and Lock

No schematic available yet, pinout is described in the [main/hardware.h](main/hardware.h).

## Running and Operation

To generate an OTP key on your computer, use the `utils/get_otp.py` script:

```sh
./utils/get_otp.py ./private/key.bin uid
```

For example, to generate an OTP key for user with uid 1, the command will look like this:

```sh
./utils/get_otp.py ./private/key.bin 1
```

The script output is a string in otpauth format:

```
otpauth://totp/1?secret=0000H1111S2222CAAUG6XXKPOBBBBXXXXXI5CQKBP6JXXXX...&issuer=Xecut
```

Which you can feed to qrencode to get a beautiful QR code that any mobile app for one-time codes will understand:

```sh
./utils/get_otp.py ./private/key.bin 1 | qrencode -t UTF8
```
