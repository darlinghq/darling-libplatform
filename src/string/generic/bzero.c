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

#if defined(__x86_64__)
#include <platform/string_avx2.h>
#endif

#if !VARIANT_STATIC
// to satisfy compiler-generated memset inside libplatform (e.g. makecontext)
__attribute__((visibility("hidden")))
void *
memset(void *b, int c, size_t len)
{
	return _platform_memset(b, c, len);
}
#endif

#if !_PLATFORM_OPTIMIZED_MEMSET

#if defined(__x86_64__)
/*
 * A fill has no overlap to preserve, so this writes 32 bytes at a time and
 * lets the last store overlap the ones before it. Only [b, b+len) is
 * written. The caller guarantees len >= PLATFORM_MEM_VECTOR.
 */
__attribute__((target("avx2"), visibility("hidden")))
void *
_platform_memset_avx2(void *b, int c, size_t len)
{
	const __m256i pattern = _mm256_set1_epi8((char)c);
	unsigned char *destination = (unsigned char *)b;
	size_t filled = 0;

	while (len - filled >= PLATFORM_MEM_STEP) {
		_mm256_storeu_si256((__m256i *)(void *)(destination + filled), pattern);
		_mm256_storeu_si256((__m256i *)(void *)(destination + filled + PLATFORM_MEM_VECTOR), pattern);
		filled += PLATFORM_MEM_STEP;
	}
	while (len - filled >= PLATFORM_MEM_VECTOR) {
		_mm256_storeu_si256((__m256i *)(void *)(destination + filled), pattern);
		filled += PLATFORM_MEM_VECTOR;
	}
	if (filled != len)
		_mm256_storeu_si256((__m256i *)(void *)(destination + len - PLATFORM_MEM_VECTOR), pattern);
	_mm256_zeroupper();
	return (b);
}
#endif /* __x86_64__ */

void *
_platform_memset(void *b, int c, size_t len) {
	unsigned char pattern[4];

#if defined(__x86_64__)
	if (platform_has_avx2()) {
		if (len >= PLATFORM_MEM_VECTOR)
			return _platform_memset_avx2(b, c, len);
		/* The doubling fill below costs two memmove calls per call; a
		   short fill writes the bytes directly instead. */
		platform_fill_short(b, (unsigned char)c, len);
		return b;
	}
#endif

	pattern[0] = (unsigned char)c;
	pattern[1] = (unsigned char)c;
	pattern[2] = (unsigned char)c;
	pattern[3] = (unsigned char)c;

	_platform_memset_pattern4(b, pattern, len);
	return b;
}

#if VARIANT_STATIC
void *
memset(void *b, int c, size_t len) {
	return _platform_memset(b, c, len);
}
#endif

#endif


#if !_PLATFORM_OPTIMIZED_BZERO

void
_platform_bzero(void *s, size_t n)
{
	_platform_memset(s, 0, n);
}

#if VARIANT_STATIC
void
bzero(void *s, size_t n) {
	_platform_bzero(s, n);
}

void
__bzero(void *s, size_t n) {
	_platform_bzero(s, n);
}
#endif

#endif
