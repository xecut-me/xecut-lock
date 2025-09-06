/*
 * qrencode - QR Code encoder
 *
 * Binary sequence class.
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

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

typedef struct {
	void *(*malloc)(size_t size);
	void *(*realloc)(void *ptr, size_t new_size);
	void  (*free)(void *);
} Allocator;

extern Allocator qrenc_alloc;

#endif /* ALLOCATOR_H */
