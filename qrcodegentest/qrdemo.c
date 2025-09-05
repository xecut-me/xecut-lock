#include <stdio.h>
#include <string.h>

#include "qrencode.h"
#include "split.h"

static const char *text = "otpauth://totp/Xecut%3Atest_user?period=30&digits=6&algorithm=SHA1&secret=AAOAAOOAALLAAQMM&issuer=Xecut";

static int margin = 2;

static void writeUTF8_margin(int realwidth, const char* white,
                             const char *reset, const char* full)
{
	int x, y;

	for (y = 0; y < margin/2; y++) {
		printf("%s", white);
		for (x = 0; x < realwidth; x++) {
			printf("%s", full);
		}
		printf("%s", reset);
		printf("\n");
	}
}

static int writeUTF8(const QRcode *qrcode)
{
	int x, y;
	int realwidth;
	const char *white, *reset;
	const char *empty, *lowhalf, *uphalf, *full;

	empty = " ";
	lowhalf = "\342\226\204";
	uphalf = "\342\226\200";
	full = "\342\226\210";

	white = "";
	reset = "";

	realwidth = (qrcode->width + margin * 2);

	/* top margin */
	writeUTF8_margin(realwidth, white, reset, full);

	/* data */
	for(y = 0; y < qrcode->width; y += 2) {
		unsigned char *row1, *row2;
		row1 = qrcode->data + y*qrcode->width;
		row2 = row1 + qrcode->width;

		printf("%s", white);

		for (x = 0; x < margin; x++) {
			printf("%s", full);
		}

		for (x = 0; x < qrcode->width; x++) {
			if(row1[x] & 1) {
				if(y < qrcode->width - 1 && row2[x] & 1) {
					printf("%s", empty);
				} else {
					printf("%s", lowhalf);
				}
			} else if(y < qrcode->width - 1 && row2[x] & 1) {
				printf("%s", uphalf);
			} else {
				printf("%s", full);
			}
		}

		for (x = 0; x < margin; x++)
			printf("%s", full);

		printf("%s", reset);
		printf("\n");
	}

	/* bottom margin */
	writeUTF8_margin(realwidth, white, reset, full);

	return 0;
}

int main(int argc, char **argv)
{
	QRinput *input;
	QRcode *code;
	int ret;

	input = QRinput_new(/* version */ 0);
	if(input == NULL) return 1;

	ret = Split_splitStringToQRinput(text, input);
	if(ret < 0) {
		QRinput_free(input);
		return 2;
	}

	code = QRcode_encodeInput(input);
	QRinput_free(input);

	writeUTF8(code);

	return 0;
}