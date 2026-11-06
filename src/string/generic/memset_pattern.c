/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN4 || !_PLATFORM_OPTIMIZED_MEMSET_PATTERN8 || !_PLATFORM_OPTIMIZED_MEMSET_PATTERN16

/*
 * Copy the pattern once, then repeatedly double the filled part of the
 * buffer. This takes O(log(len / size)) memmove calls instead of one per
 * pattern repetition, and the pattern is read only before the buffer is
 * written, so a pattern that lies inside the buffer is still used as it was
 * on entry.
 */
static void
_platform_memset_pattern_fill(void *b, const void *pattern, size_t len, size_t size)
{
	unsigned char saved[16];
	unsigned char *dst = (unsigned char *)b;
	size_t filled;

	if (len == 0) {
		return;
	}

	_platform_memmove(saved, pattern, size);
	filled = (len < size) ? len : size;
	_platform_memmove(dst, saved, filled);

	while (filled < len) {
		size_t chunk = (filled < len - filled) ? filled : len - filled;
		_platform_memmove(dst + filled, dst, chunk);
		filled += chunk;
	}
}

#endif

#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN4

void
_platform_memset_pattern4(void *b, const void *pattern4, size_t len)
{
	_platform_memset_pattern_fill(b, pattern4, len, 4);
}

#if VARIANT_STATIC
void
memset_pattern4(void *b, const void *pattern4, size_t len)
{
	return _platform_memset_pattern4(b, pattern4, len);
}
#endif

#endif


#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN8

void
_platform_memset_pattern8(void *b, const void *pattern8, size_t len)
{
	_platform_memset_pattern_fill(b, pattern8, len, 8);
}

#if VARIANT_STATIC
void
memset_pattern8(void *b, const void *pattern8, size_t len)
{
	return _platform_memset_pattern8(b, pattern8, len);
}
#endif

#endif


#if !_PLATFORM_OPTIMIZED_MEMSET_PATTERN16

void
_platform_memset_pattern16(void *b, const void *pattern16, size_t len)
{
	_platform_memset_pattern_fill(b, pattern16, len, 16);
}

#if VARIANT_STATIC
void
memset_pattern16(void *b, const void *pattern16, size_t len)
{
	return _platform_memset_pattern16(b, pattern16, len);
}
#endif

#endif
