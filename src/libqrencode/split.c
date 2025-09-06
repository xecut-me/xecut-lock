/*
 * qrencode - QR Code encoder
 *
 * Input data splitter.
 * Copyright (C) 2006-2017 Kentaro Fukuchi <kentaro@fukuchi.org>
 *
 * The following data / specifications are taken from
 * "Two dimensional symbol -- QR-code -- Basic Specification" (JIS X0510:2004)
 *  or
 * "Automatic identification and data capture techniques --
 *  QR Code 2005 bar code symbology specification" (ISO/IEC 18004:2006)
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "qrencode.h"
#include "qrinput.h"
#include "qrspec.h"
#include "split.h"

#define isdigit(__c__) ((unsigned char)((signed char)(__c__) - '0') < 10)
#define isalnum(__c__) (QRinput_lookAnTable(__c__) >= 0)

static QRencodeMode Split_identifyMode(const char *string)
{
	unsigned char c = (unsigned char)string[0];

	if(c == '\0') return QR_MODE_NUL;
	if(isdigit(c)) {
		return QR_MODE_NUM;
	} else if(isalnum(c)) {
		return QR_MODE_AN;
	}

	return QR_MODE_8;
}

static int Split_eatAn(const char *string, QRinput *input);
static int Split_eat8(const char *string, QRinput *input);

static int Split_eatNum(const char *string, QRinput *input)
{
	const char *p;
	int ret;
	int run;
	int dif;
	int ln;
	QRencodeMode mode;

	ln = QRspec_lengthIndicator(QR_MODE_NUM, input->version);

	p = string;
	while(isdigit(*p)) {
		p++;
	}
	run = (int)(p - string);
	mode = Split_identifyMode(p);
	if(mode == QR_MODE_8) {
		dif = QRinput_estimateBitsModeNum(run) + 4 + ln
			+ QRinput_estimateBitsMode8(1) /* + 4 + l8 */
			- QRinput_estimateBitsMode8(run + 1) /* - 4 - l8 */;
		if(dif > 0) {
			return Split_eat8(string, input);
		}
	}
	if(mode == QR_MODE_AN) {
		dif = QRinput_estimateBitsModeNum(run) + 4 + ln
			+ QRinput_estimateBitsModeAn(1) /* + 4 + la */
			- QRinput_estimateBitsModeAn(run + 1) /* - 4 - la */;
		if(dif > 0) {
			return Split_eatAn(string, input);
		}
	}

	ret = QRinput_append(input, QR_MODE_NUM, run, (unsigned char *)string);
	if(ret < 0) return -1;

	return run;
}

static int Split_eatAn(const char *string, QRinput *input)
{
	const char *p, *q;
	int ret;
	int run;
	int dif;
	int la, ln;

	la = QRspec_lengthIndicator(QR_MODE_AN, input->version);
	ln = QRspec_lengthIndicator(QR_MODE_NUM, input->version);

	p = string;
	while(isalnum(*p)) {
		if(isdigit(*p)) {
			q = p;
			while(isdigit(*q)) {
				q++;
			}
			dif = QRinput_estimateBitsModeAn((int)(p - string)) /* + 4 + la */
				+ QRinput_estimateBitsModeNum((int)(q - p)) + 4 + ln
				+ (isalnum(*q)?(4 + ln):0)
				- QRinput_estimateBitsModeAn((int)(q - string)) /* - 4 - la */;
			if(dif < 0) {
				break;
			}
			p = q;
		} else {
			p++;
		}
	}

	run = (int)(p - string);

	if(*p && !isalnum(*p)) {
		dif = QRinput_estimateBitsModeAn(run) + 4 + la
			+ QRinput_estimateBitsMode8(1) /* + 4 + l8 */
			- QRinput_estimateBitsMode8(run + 1) /* - 4 - l8 */;
		if(dif > 0) {
			return Split_eat8(string, input);
		}
	}

	ret = QRinput_append(input, QR_MODE_AN, run, (unsigned char *)string);
	if(ret < 0) return -1;

	return run;
}

static int Split_eat8(const char *string, QRinput *input)
{
	const char *p, *q;
	QRencodeMode mode;
	int ret;
	int run;
	int dif;
	int la, ln, l8;
	int swcost;

	la = QRspec_lengthIndicator(QR_MODE_AN, input->version);
	ln = QRspec_lengthIndicator(QR_MODE_NUM, input->version);
	l8 = QRspec_lengthIndicator(QR_MODE_8, input->version);

	p = string + 1;
	while(*p != '\0') {
		mode = Split_identifyMode(p);
		if(mode == QR_MODE_NUM) {
			q = p;
			while(isdigit(*q)) {
				q++;
			}
			if(Split_identifyMode(q) == QR_MODE_8) {
				swcost = 4 + l8;
			} else {
				swcost = 0;
			}
			dif = QRinput_estimateBitsMode8((int)(p - string)) /* + 4 + l8 */
				+ QRinput_estimateBitsModeNum((int)(q - p)) + 4 + ln
				+ swcost
				- QRinput_estimateBitsMode8((int)(q - string)) /* - 4 - l8 */;
			if(dif < 0) {
				break;
			}
			p = q;
		} else if(mode == QR_MODE_AN) {
			q = p;
			while(isalnum(*q)) {
				q++;
			}
			if(Split_identifyMode(q) == QR_MODE_8) {
				swcost = 4 + l8;
			} else {
				swcost = 0;
			}
			dif = QRinput_estimateBitsMode8((int)(p - string)) /* + 4 + l8 */
				+ QRinput_estimateBitsModeAn((int)(q - p)) + 4 + la
				+ swcost
				- QRinput_estimateBitsMode8((int)(q - string)) /* - 4 - l8 */;
			if(dif < 0) {
				break;
			}
			p = q;
		} else {
			p++;
		}
	}

	run = (int)(p - string);
	ret = QRinput_append(input, QR_MODE_8, run, (unsigned char *)string);
	if(ret < 0) return -1;

	return run;
}

int Split_splitStringToQRinput(const char *string, QRinput *input)
{
	int length;
	QRencodeMode mode;

	while(*string != '\0') {
		mode = Split_identifyMode(string);
		if(mode == QR_MODE_NUM) {
			length = Split_eatNum(string, input);
		} else if(mode == QR_MODE_AN) {
			length = Split_eatAn(string, input);
		} else {
			length = Split_eat8(string, input);
		}
		if(length == 0) break;
		if(length < 0) return -1;
		string += length;
	}

	return 0;
}
