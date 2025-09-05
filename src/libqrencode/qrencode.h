/**
 * qrencode - QR Code encoder
 *
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

#ifndef QRENCODE_H
#define QRENCODE_H

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Encoding mode.
 */
typedef enum {
	QR_MODE_NUL = -1,   ///< Terminator (NUL character). Internal use only
	QR_MODE_NUM = 0,    ///< Numeric mode
	QR_MODE_AN,         ///< Alphabet-numeric mode
	QR_MODE_8,          ///< 8-bit data mode
	QR_MODE_FNC1FIRST,  ///< FNC1, first position
	QR_MODE_FNC1SECOND, ///< FNC1, second position
} QRencodeMode;

/**
 * Maximum version (size) of QR-code symbol.
 */
#define QRSPEC_VERSION_MAX 40

/**
 * Maximum version (size) of QR-code symbol.
 */
#define MQRSPEC_VERSION_MAX 4


/******************************************************************************
 * Input data (qrinput.c)
 *****************************************************************************/

/**
 * Singly linked list to contain input strings. An instance of this class
 * contains its version and error correction level too. It is required to
 * set them by QRinput_setVersion() and QRinput_setErrorCorrectionLevel(),
 * or use QRinput_new() to instantiate an object.
 */
typedef struct _QRinput QRinput;

/**
 * Instantiate an input data object.
 * @return an input object (initialized). On error, NULL is returned and errno
 *         is set to indicate the error.
 * @throw ENOMEM unable to allocate memory for input objects.
 * @throw EINVAL invalid arguments.
 */
extern QRinput *QRinput_new(int version);

/**
 * Append data to an input object.
 * The data is copied and appended to the input object.
 * @param input input object.
 * @param mode encoding mode.
 * @param size size of data (byte).
 * @param data a pointer to the memory area of the input data.
 * @retval 0 success.
 * @retval -1 an error occurred and errno is set to indeicate the error.
 *            See Execptions for the details.
 * @throw ENOMEM unable to allocate memory.
 * @throw EINVAL input data is invalid.
 *
 */
extern int QRinput_append(QRinput *input, QRencodeMode mode, int size, const unsigned char *data);

/**
 * Get current version.
 * @param input input object.
 * @return current version.
 */
extern int QRinput_getVersion(QRinput *input);

/**
 * Set version of the QR code that is to be encoded.
 * This function cannot be applied to Micro QR Code.
 * @param input input object.
 * @param version version number (0 = auto)
 * @retval 0 success.
 * @retval -1 invalid argument.
 */
extern int QRinput_setVersion(QRinput *input, int version);

/**
 * Free the input object.
 * All of data chunks in the input object are freed too.
 * @param input input object.
 */
extern void QRinput_free(QRinput *input);

/**
 * Validate the input data.
 * @param mode encoding mode.
 * @param size size of data (byte).
 * @param data a pointer to the memory area of the input data.
 * @retval 0 success.
 * @retval -1 invalid arguments.
 */
extern int QRinput_check(QRencodeMode mode, int size, const unsigned char *data);

/******************************************************************************
 * QRcode output (qrencode.c)
 *****************************************************************************/

/**
 * QRcode class.
 * Symbol data is represented as an array contains width*width uchars.
 * Each uchar represents a module (dot). If the less significant bit of
 * the uchar is 1, the corresponding module is black. The other bits are
 * meaningless for usual applications, but here its specification is described.
 *
 * @verbatim
   MSB 76543210 LSB
       |||||||`- 1=black/0=white
       ||||||`-- 1=ecc/0=data code area
       |||||`--- format information
       ||||`---- version information
       |||`----- timing pattern
       ||`------ alignment pattern
       |`------- finder pattern and separator
       `-------- non-data modules (format, timing, etc.)
   @endverbatim
 */
typedef struct {
	int version;         ///< version of the symbol
	int width;           ///< width of the symbol
	unsigned char *data; ///< symbol data
} QRcode;

/**
 * Singly-linked list of QRcode. Used to represent a structured symbols.
 * A list is terminated with NULL.
 */
typedef struct _QRcode_List {
	QRcode *code;
	struct _QRcode_List *next;
} QRcode_List;

/**
 * Create a symbol from the input data.
 * @warning This function is THREAD UNSAFE when pthread is disabled.
 * @param input input data.
 * @return an instance of QRcode class. The version of the result QRcode may
 *         be larger than the designated version. On error, NULL is returned,
 *         and errno is set to indicate the error. See Exceptions for the
 *         details.
 * @throw EINVAL invalid input object.
 * @throw ENOMEM unable to allocate memory for input objects.
 */
extern QRcode *QRcode_encodeInput(QRinput *input);

/**
 * Free the instance of QRcode class.
 * @param qrcode an instance of QRcode class.
 */
extern void QRcode_free(QRcode *qrcode);

#if defined(__cplusplus)
}
#endif

#endif /* QRENCODE_H */
