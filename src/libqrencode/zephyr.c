#include "zephyr.h"
#include "allocator.h"
#include "split.h"
#include "qrencode.h"

#include <zephyr/kernel.h>

#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>

#include <stdlib.h>
#include <stdio.h>

/******************************************************************************
 * Allocator
 *****************************************************************************/

#define QRENC_HEAP_SIZE (6 * 1024)

K_HEAP_DEFINE(qrenc_heap, QRENC_HEAP_SIZE);
static void *qrenc_buffer = NULL;

void *qrenc_alloc_malloc(size_t size) {
	return k_heap_alloc(&qrenc_heap, size, K_NO_WAIT);
}

void *qrenc_alloc_realloc(void *ptr, size_t new_size) {
	return k_heap_realloc(&qrenc_heap, ptr, new_size, K_NO_WAIT);
}

void qrenc_alloc_free(void *ptr) {
	return k_heap_free(&qrenc_heap, ptr);
}

Allocator qrenc_alloc = {
	.malloc  = qrenc_alloc_malloc,
	.realloc = qrenc_alloc_realloc,
	.free    = qrenc_alloc_free,
};

void qrenc_alloc_init(void) {
	if (qrenc_buffer == NULL) {
		qrenc_buffer = k_malloc(QRENC_HEAP_SIZE);
	}

	// Reinitialize the heap to eliminate potential defragmentation
	// when generating codes multiple times.
	k_heap_init(&qrenc_heap, qrenc_buffer, QRENC_HEAP_SIZE);
}

/******************************************************************************
 * QR Code visualizer
 *****************************************************************************/

 static int qrcode_margin = 2;

// Copied from:
// https://github.com/fukuchi/libqrencode/blob/715e29fd4cd71b6e452ae0f4e36d917b43122ce8/qrenc.c#L833
static void print_qrcode_margin(int realwidth, const char* full) {
	for (int y = 0; y < qrcode_margin / 2; y++) {
		for (int x = 0; x < realwidth; x++) {
			fputs(full, stdout);
		}
		fputc('\n', stdout);
	}
}

// Copied from:
// https://github.com/fukuchi/libqrencode/blob/715e29fd4cd71b6e452ae0f4e36d917b43122ce8/qrenc.c#L847
static int print_qrcode(const QRcode *qrcode)
{
	const char *empty = " ";
	const char *lowhalf = "\342\226\204";
	const char *uphalf = "\342\226\200";
	const char *full = "\342\226\210";

	int realwidth = (qrcode->width + qrcode_margin * 2);

	/* top margin */
	print_qrcode_margin(realwidth, full);

	/* data */
	for (int y = 0; y < qrcode->width; y += 2) {
		unsigned char *row1 = qrcode->data + y*qrcode->width;
		unsigned char *row2 = row1 + qrcode->width;

		for (int x = 0; x < qrcode_margin; x++) {
			fputs(full, stdout);
		}

		for (int x = 0; x < qrcode->width; x++) {
			if (row1[x] & 1) {
				if (y < qrcode->width - 1 && row2[x] & 1) {
					fputs(empty, stdout);
				} else {
					fputs(lowhalf, stdout);
				}
			} else if(y < qrcode->width - 1 && row2[x] & 1) {
				fputs(uphalf, stdout);
			} else {
				fputs(full, stdout);
			}
		}

		for (int x = 0; x < qrcode_margin; x++) {
			fputs(full, stdout);
		}

		fputc('\n', stdout);
	}

	/* bottom margin */
	print_qrcode_margin(realwidth, full);

	return 0;
}

/******************************************************************************
 * QR Code generator
 *****************************************************************************/

static QRcode *create_qrcode(const char *text) {
	QRinput *input = QRinput_new(QR_VERSION_AUTO);
	__ASSERT(input != NULL, "Failed to alloc memory for QRinput");

	int split_result = Split_splitStringToQRinput(text, input);
	__ASSERT(split_result == 0, "Failed to prepare data for qrcode");

	QRcode *code = QRcode_encodeInput(input);
	__ASSERT(code != NULL, "Failed to alloc memory for QRcode");

	QRinput_free(input);

	return code;
}

/******************************************************************************
 * Public API
 *****************************************************************************/

void display_qr_code(const char *text) {
	qrenc_alloc_init();

    QRcode *code = create_qrcode(text);

	print_qrcode(code);

	QRcode_free(code);
}
