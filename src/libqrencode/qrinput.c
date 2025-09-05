/*
 * qrencode - QR Code encoder
 *
 * Input data chunk class
 * Copyright (C) 2006-2017 Kentaro Fukuchi <kentaro@fukuchi.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "qrencode.h"
#include "qrspec.h"
#include "bitstream.h"
#include "qrinput.h"

/******************************************************************************
 * Utilities
 *****************************************************************************/
int QRinput_isSplittableMode(QRencodeMode mode)
{
	return (mode >= QR_MODE_NUM && mode < QR_MODE_FNC1FIRST);
}

/******************************************************************************
 * Entry of input data
 *****************************************************************************/

static QRinput_List *QRinput_List_newEntry(QRencodeMode mode, int size, const unsigned char *data)
{
	QRinput_List *entry;

	if(QRinput_check(mode, size, data)) {
		errno = EINVAL;
		return NULL;
	}

	entry = (QRinput_List *)malloc(sizeof(QRinput_List));
	if(entry == NULL) return NULL;

	entry->mode = mode;
	entry->size = size;
	entry->data = NULL;
	if(size > 0) {
		entry->data = (unsigned char *)malloc((size_t)size);
		if(entry->data == NULL) {
			free(entry);
			return NULL;
		}
		memcpy(entry->data, data, (size_t)size);
	}
	entry->bstream = NULL;
	entry->next = NULL;

	return entry;
}

static void QRinput_List_freeEntry(QRinput_List *entry)
{
	if(entry != NULL) {
		free(entry->data);
		BitStream_free(entry->bstream);
		free(entry);
	}
}

static QRinput_List *QRinput_List_dup(QRinput_List *entry)
{
	QRinput_List *n;

	n = (QRinput_List *)malloc(sizeof(QRinput_List));
	if(n == NULL) return NULL;

	n->mode = entry->mode;
	n->size = entry->size;
	n->data = (unsigned char *)malloc((size_t)n->size);
	if(n->data == NULL) {
		free(n);
		return NULL;
	}
	memcpy(n->data, entry->data, (size_t)entry->size);
	n->bstream = NULL;
	n->next = NULL;

	return n;
}

/******************************************************************************
 * Input Data
 *****************************************************************************/

QRinput *QRinput_new(int version)
{
	QRinput *input;

	input = (QRinput *)malloc(sizeof(QRinput));
	if(input == NULL) return NULL;

	input->head = NULL;
	input->tail = NULL;
	input->version = version;
	input->fnc1 = 0;

	return input;
}

int QRinput_getVersion(QRinput *input)
{
	return input->version;
}

int QRinput_setVersion(QRinput *input, int version)
{
	if(version < 0 || version > QRSPEC_VERSION_MAX) {
		errno = EINVAL;
		return -1;
	}

	input->version = version;

	return 0;
}

static void QRinput_appendEntry(QRinput *input, QRinput_List *entry)
{
	if(input->tail == NULL) {
		input->head = entry;
		input->tail = entry;
	} else {
		input->tail->next = entry;
		input->tail = entry;
	}
	entry->next = NULL;
}

int QRinput_append(QRinput *input, QRencodeMode mode, int size, const unsigned char *data)
{
	QRinput_List *entry;

	entry = QRinput_List_newEntry(mode, size, data);
	if(entry == NULL) {
		return -1;
	}

	QRinput_appendEntry(input, entry);

	return 0;
}

void QRinput_free(QRinput *input)
{
	QRinput_List *list, *next;

	if(input != NULL) {
		list = input->head;
		while(list != NULL) {
			next = list->next;
			QRinput_List_freeEntry(list);
			list = next;
		}
		free(input);
	}
}

QRinput *QRinput_dup(QRinput *input)
{
	QRinput *n;
	QRinput_List *list, *e;

	n = QRinput_new(input->version);
	if(n == NULL) return NULL;

	list = input->head;
	while(list != NULL) {
		e = QRinput_List_dup(list);
		if(e == NULL) {
			QRinput_free(n);
			return NULL;
		}
		QRinput_appendEntry(n, e);
		list = list->next;
	}

	return n;
}

/******************************************************************************
 * Numeric data
 *****************************************************************************/

/**
 * Check the input data.
 * @param size
 * @param data
 * @return result
 */
static int QRinput_checkModeNum(int size, const char *data)
{
	int i;

	for(i = 0; i < size; i++) {
		if(data[i] < '0' || data[i] > '9')
			return -1;
	}

	return 0;
}

/**
 * Estimate the length of the encoded bit stream of numeric data.
 * @param size
 * @return number of bits
 */
int QRinput_estimateBitsModeNum(int size)
{
	int w;
	int bits;

	w = size / 3;
	bits = w * 10;
	switch(size - w * 3) {
		case 1:
			bits += 4;
			break;
		case 2:
			bits += 7;
			break;
		default:
			break;
	}

	return bits;
}

/**
 * Convert the number data and append to a bit stream.
 * @param entry
 * @retval 0 success
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ENOMEM unable to allocate memory.
 */
static int QRinput_encodeModeNum(QRinput_List *entry, BitStream *bstream, int version)
{
	int words, i, ret;
	unsigned int val;

	ret = BitStream_appendNum(bstream, 4, QRSPEC_MODEID_NUM);
	if(ret < 0) return -1;

	ret = BitStream_appendNum(bstream, (size_t)QRspec_lengthIndicator(QR_MODE_NUM, version), (unsigned int)entry->size);
	if(ret < 0) return -1;

	words = entry->size / 3;
	for(i = 0; i < words; i++) {
		val  = (unsigned int)(entry->data[i*3  ] - '0') * 100;
		val += (unsigned int)(entry->data[i*3+1] - '0') * 10;
		val += (unsigned int)(entry->data[i*3+2] - '0');

		ret = BitStream_appendNum(bstream, 10, val);
		if(ret < 0) return -1;
	}

	if(entry->size - words * 3 == 1) {
		val = (unsigned int)(entry->data[words*3] - '0');
		ret = BitStream_appendNum(bstream, 4, val);
		if(ret < 0) return -1;
	} else if(entry->size - words * 3 == 2) {
		val  = (unsigned int)(entry->data[words*3  ] - '0') * 10;
		val += (unsigned int)(entry->data[words*3+1] - '0');
		ret = BitStream_appendNum(bstream, 7, val);
		if(ret < 0) return -1;
	}

	return 0;
}

/******************************************************************************
 * Alphabet-numeric data
 *****************************************************************************/

const signed char QRinput_anTable[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	36, -1, -1, -1, 37, 38, -1, -1, -1, -1, 39, 40, -1, 41, 42, 43,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 44, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/**
 * Check the input data.
 * @param size
 * @param data
 * @return result
 */
static int QRinput_checkModeAn(int size, const char *data)
{
	int i;

	for(i = 0; i < size; i++) {
		if(QRinput_lookAnTable(data[i]) < 0)
			return -1;
	}

	return 0;
}

/**
 * Estimate the length of the encoded bit stream of alphabet-numeric data.
 * @param size
 * @return number of bits
 */
int QRinput_estimateBitsModeAn(int size)
{
	int w;
	int bits;

	w = size / 2;
	bits = w * 11;
	if(size & 1) {
		bits += 6;
	}

	return bits;
}

/**
 * Convert the alphabet-numeric data and append to a bit stream.
 * @param entry
 * @retval 0 success
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ENOMEM unable to allocate memory.
 * @throw EINVAL invalid version.
 */
static int QRinput_encodeModeAn(QRinput_List *entry, BitStream *bstream, int version)
{
	int words, i, ret;
	unsigned int val;

	ret = BitStream_appendNum(bstream, 4, QRSPEC_MODEID_AN);
	if(ret < 0) return -1;
	ret = BitStream_appendNum(bstream, (size_t)QRspec_lengthIndicator(QR_MODE_AN, version), (unsigned int)entry->size);
	if(ret < 0) return -1;

	words = entry->size / 2;
	for(i = 0; i < words; i++) {
		val  = (unsigned int)QRinput_lookAnTable(entry->data[i*2  ]) * 45;
		val += (unsigned int)QRinput_lookAnTable(entry->data[i*2+1]);

		ret = BitStream_appendNum(bstream, 11, val);
		if(ret < 0) return -1;
	}

	if(entry->size & 1) {
		val = (unsigned int)QRinput_lookAnTable(entry->data[words * 2]);

		ret = BitStream_appendNum(bstream, 6, val);
		if(ret < 0) return -1;
	}

	return 0;
}

/******************************************************************************
 * 8 bit data
 *****************************************************************************/

/**
 * Estimate the length of the encoded bit stream of 8 bit data.
 * @param size
 * @return number of bits
 */
int QRinput_estimateBitsMode8(int size)
{
	return size * 8;
}

/**
 * Convert the 8bits data and append to a bit stream.
 * @param entry
 * @retval 0 success
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ENOMEM unable to allocate memory.
 */
static int QRinput_encodeMode8(QRinput_List *entry, BitStream *bstream, int version)
{
	int ret;

	ret = BitStream_appendNum(bstream, 4, QRSPEC_MODEID_8);
	if(ret < 0) return -1;

	ret = BitStream_appendNum(bstream, (size_t)QRspec_lengthIndicator(QR_MODE_8, version), (unsigned int)entry->size);
	if(ret < 0) return -1;

	ret = BitStream_appendBytes(bstream, (size_t)entry->size, entry->data);
	if(ret < 0) return -1;

	return 0;
}

/******************************************************************************
 * FNC1
 *****************************************************************************/

static int QRinput_checkModeFNC1Second(int size)
{
	if(size != 1) return -1;

	/* No data check required. */

	return 0;
}

static int QRinput_encodeModeFNC1Second(QRinput_List *entry, BitStream *bstream)
{
	int ret;

	ret = BitStream_appendNum(bstream, 4, QRSPEC_MODEID_FNC1SECOND);
	if(ret < 0) return -1;

	ret = BitStream_appendBytes(bstream, 1, entry->data);
	if(ret < 0) return -1;

	return 0;
}

/******************************************************************************
 * Validation
 *****************************************************************************/

int QRinput_check(QRencodeMode mode, int size, const unsigned char *data)
{
	if((mode == QR_MODE_FNC1FIRST && size < 0) || size <= 0) return -1;

	switch(mode) {
		case QR_MODE_NUM:
			return QRinput_checkModeNum(size, (const char *)data);
		case QR_MODE_AN:
			return QRinput_checkModeAn(size, (const char *)data);
		case QR_MODE_8:
			return 0;
		case QR_MODE_FNC1FIRST:
			return 0;
		case QR_MODE_FNC1SECOND:
			return QRinput_checkModeFNC1Second(size);
		case QR_MODE_NUL:
			break;
	}

	return -1;
}

/******************************************************************************
 * Estimation of the bit length
 *****************************************************************************/

/**
 * Estimate the length of the encoded bit stream on the current version.
 * @param entry
 * @param version version of the symbol
 * @return number of bits
 */
static int QRinput_estimateBitStreamSizeOfEntry(QRinput_List *entry, int version)
{
	int bits = 0;
	int l, m;
	int num;

	if(version == 0) version = 1;

	switch(entry->mode) {
		case QR_MODE_NUM:
			bits = QRinput_estimateBitsModeNum(entry->size);
			break;
		case QR_MODE_AN:
			bits = QRinput_estimateBitsModeAn(entry->size);
			break;
		case QR_MODE_8:
			bits = QRinput_estimateBitsMode8(entry->size);
			break;
		case QR_MODE_FNC1FIRST:
			return MODE_INDICATOR_SIZE;
		case QR_MODE_FNC1SECOND:
			return MODE_INDICATOR_SIZE + 8;
		default:
			return 0;
	}

	l = QRspec_lengthIndicator(entry->mode, version);
	m = 1 << l;
	num = (entry->size + m - 1) / m;

	bits += num * (MODE_INDICATOR_SIZE + l);

	return bits;
}

/**
 * Estimate the length of the encoded bit stream of the data.
 * @param input input data
 * @param version version of the symbol
 * @return number of bits
 */
int QRinput_estimateBitStreamSize(QRinput *input, int version)
{
	QRinput_List *list;
	int bits = 0;

	list = input->head;
	while(list != NULL) {
		bits += QRinput_estimateBitStreamSizeOfEntry(list, version);
		list = list->next;
	}

	return bits;
}

/**
 * Estimate the required version number of the symbol.
 * @param input input data
 * @return required version number or -1 for failure.
 */
int QRinput_estimateVersion(QRinput *input)
{
	int bits;
	int version, prev;

	version = 0;
	do {
		prev = version;
		bits = QRinput_estimateBitStreamSize(input, prev);
		version = QRspec_getMinimumVersion((bits + 7) / 8);
		if(prev == 0 && version > 1) {
			version--;
		}
	} while (version > prev);

	return version;
}

/**
 * Return required length in bytes for specified mode, version and bits.
 * @param mode
 * @param version
 * @param bits
 * @return required length of code words in bytes.
 */
int QRinput_lengthOfCode(QRencodeMode mode, int version, int bits)
{
	int payload, size, chunks, remain, maxsize;

	payload = bits - 4 - QRspec_lengthIndicator(mode, version);
	switch(mode) {
		case QR_MODE_NUM:
			chunks = payload / 10;
			remain = payload - chunks * 10;
			size = chunks * 3;
			if(remain >= 7) {
				size += 2;
			} else if(remain >= 4) {
				size += 1;
			}
			break;
		case QR_MODE_AN:
			chunks = payload / 11;
			remain = payload - chunks * 11;
			size = chunks * 2;
			if(remain >= 6) size++;
			break;
		case QR_MODE_8:
			size = payload / 8;
			break;
		default:
			size = 0;
			break;
	}
	maxsize = QRspec_maximumWords(mode, version);
	if(size < 0) size = 0;
	if(maxsize > 0 && size > maxsize) size = maxsize;

	return size;
}

/******************************************************************************
 * Data conversion
 *****************************************************************************/

/**
 * Convert the input data in the data chunk and append to a bit stream.
 * @param entry
 * @param bstream
 * @return number of bits (>0) or -1 for failure.
 */
static int QRinput_encodeBitStream(QRinput_List *entry, BitStream *bstream, int version)
{
	int words, ret;
	QRinput_List *st1 = NULL, *st2 = NULL;
	int prevsize;

	prevsize = (int)BitStream_size(bstream);

	words = QRspec_maximumWords(entry->mode, version);
	if(words != 0 && entry->size > words) {
		st1 = QRinput_List_newEntry(entry->mode, words, entry->data);
		if(st1 == NULL) goto ABORT;
		st2 = QRinput_List_newEntry(entry->mode, entry->size - words, &entry->data[words]);
		if(st2 == NULL) goto ABORT;

		ret = QRinput_encodeBitStream(st1, bstream, version);
		if(ret < 0) goto ABORT;
		ret = QRinput_encodeBitStream(st2, bstream, version);
		if(ret < 0) goto ABORT;

		QRinput_List_freeEntry(st1);
		QRinput_List_freeEntry(st2);
	} else {
		ret = 0;
		switch(entry->mode) {
			case QR_MODE_NUM:
				ret = QRinput_encodeModeNum(entry, bstream, version);
				break;
			case QR_MODE_AN:
				ret = QRinput_encodeModeAn(entry, bstream, version);
				break;
			case QR_MODE_8:
				ret = QRinput_encodeMode8(entry, bstream, version);
				break;
			case QR_MODE_FNC1SECOND:
				ret = QRinput_encodeModeFNC1Second(entry, bstream);
				break;
			default:
				break;
		}
		if(ret < 0) return -1;
	}

	return (int)BitStream_size(bstream) - prevsize;
ABORT:
	QRinput_List_freeEntry(st1);
	QRinput_List_freeEntry(st2);
	return -1;
}

/**
 * Convert the input data to a bit stream.
 * @param input input data.
 * @retval 0 success
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ENOMEM unable to allocate memory.
 */
static int QRinput_createBitStream(QRinput *input, BitStream *bstream)
{
	QRinput_List *list;
	int bits, total = 0;

	list = input->head;
	while(list != NULL) {
		bits = QRinput_encodeBitStream(list, bstream, input->version);
		if(bits < 0) return -1;
		total += bits;
		list = list->next;
	}

	return total;
}

/**
 * Convert the input data to a bit stream.
 * When the version number is given and that is not sufficient, it is increased
 * automatically.
 * @param input input data.
 * @param bstream where the converted data is stored.
 * @retval 0 success
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ENOMEM unable to allocate memory.
 * @throw ERANGE input data is too large.
 */
static int QRinput_convertData(QRinput *input, BitStream *bstream)
{
	int bits;
	int ver;

	ver = QRinput_estimateVersion(input);
	if(ver > QRinput_getVersion(input)) {
		QRinput_setVersion(input, ver);
	}

	for(;;) {
		BitStream_reset(bstream);
		bits = QRinput_createBitStream(input, bstream);
		if(bits < 0) return -1;
		ver = QRspec_getMinimumVersion((bits + 7) / 8);
		if(ver > QRinput_getVersion(input)) {
			QRinput_setVersion(input, ver);
		} else {
			break;
		}
	}

	return 0;
}

/**
 * Append padding bits for the input data.
 * @param bstream Bitstream to be appended.
 * @param input input data.
 * @retval 0 success
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ERANGE input data is too large.
 * @throw ENOMEM unable to allocate memory.
 */
static int QRinput_appendPaddingBit(BitStream *bstream, QRinput *input)
{
	int bits, maxbits, words, maxwords, i, ret;
	int padlen;

	bits = (int)BitStream_size(bstream);
	maxwords = QRspec_getDataLength(input->version);
	maxbits = maxwords * 8;

	if(maxbits < bits) {
		errno = ERANGE;
		return -1;
	}
	if(maxbits == bits) {
		return 0;
	}

	if(maxbits - bits <= 4) {
		return (int)BitStream_appendNum(bstream, (size_t)(maxbits - bits), 0);
	}

	words = (bits + 4 + 7) / 8;

	ret = (int)BitStream_appendNum(bstream, (size_t)(words * 8 - bits), 0);
	if(ret < 0) return ret;

	padlen = maxwords - words;
	if(padlen > 0) {
		for(i = 0; i < padlen; i++) {
			ret = (int)BitStream_appendNum(bstream, 8, (i&1)?0x11:0xec);
			if(ret < 0) {
				return ret;
			}
		}
	}

	return 0;
}

static int QRinput_insertFNC1Header(QRinput *input)
{
	QRinput_List *entry = NULL;

	if(input->fnc1 == 1) {
		entry = QRinput_List_newEntry(QR_MODE_FNC1FIRST, 0, NULL);
	} else if(input->fnc1 == 2) {
		entry = QRinput_List_newEntry(QR_MODE_FNC1SECOND, 1, &(input->appid));
	}
	if(entry == NULL) {
		return -1;
	}

	entry->next = input->head;
	input->head = entry;

	return 0;
}

/**
 * Merge all bit streams in the input data.
 * @param input input data.
 * @return merged bit stream
 */

int QRinput_mergeBitStream(QRinput *input, BitStream *bstream)
{
	if(input->fnc1) {
		if(QRinput_insertFNC1Header(input) < 0) {
			return -1;
		}
	}
	if(QRinput_convertData(input, bstream) < 0) {
		return -1;
	}

	return 0;
}

/**
 * Merge all bit streams in the input data and append padding bits
 * @param input input data.
 * @return padded merged bit stream
 */

int QRinput_getBitStream(QRinput *input, BitStream *bstream)
{
	int ret;

	ret = QRinput_mergeBitStream(input, bstream);
	if(ret < 0) return -1;

	ret = QRinput_appendPaddingBit(bstream, input);
	if(ret < 0) return -1;

	return 0;
}

/**
 * Pack all bit streams padding bits into a byte array.
 * @param input input data.
 * @return padded merged byte stream
 */

unsigned char *QRinput_getByteStream(QRinput *input)
{
	BitStream *bstream;
	unsigned char *array;
	int ret;

	bstream = BitStream_new();
	if(bstream == NULL) {
		return NULL;
	}

	ret = QRinput_getBitStream(input, bstream);
	if(ret < 0) {
		BitStream_free(bstream);
		return NULL;
	}
	array = BitStream_toByte(bstream);
	BitStream_free(bstream);

	return array;
}
