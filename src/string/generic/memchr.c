/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <platform/string.h>

#if !_PLATFORM_OPTIMIZED_MEMCHR

#include <stdlib.h>

#if defined(__x86_64__)
#include <platform/string_avx2.h>

/* The bits of the first count bytes of a 32-byte block, count <= 32. */
static inline unsigned
platform_memchr_low_bytes(size_t count)
{
	return (count >= PLATFORM_MEM_VECTOR ? 0xffffffffu : (1u << count) - 1);
}

/*
 * memchr must behave as if it read the bytes one at a time and stopped at
 * the first match (C11 7.24.5.1), so a caller may pass a length that runs
 * past the end of a mapping as long as the byte is found before it.
 * Therefore every load here is a 32-byte aligned block: such a block never
 * crosses a page boundary, and each block holds at least one byte that the
 * sequential scan would have read before stopping. Match bits for bytes
 * before s or after s+n are masked off, and blocks are scanned in address
 * order, so the lowest remaining bit is the first match.
 */
__attribute__((target("avx2"), visibility("hidden")))
void *
_platform_memchr_avx2(const void *s, int c, size_t n)
{
	const __m256i needle = _mm256_set1_epi8((char)c);
	const size_t offset = (uintptr_t)s & (PLATFORM_MEM_VECTOR - 1);
	const unsigned char *p = (const unsigned char *)s - offset;
	size_t remaining;
	unsigned equal;

	/* Bytes from p to the end of the range. A length that would wrap
	   cannot be reached by the scan anyway. */
	if (n > SIZE_MAX - PLATFORM_MEM_VECTOR)
		n = SIZE_MAX - PLATFORM_MEM_VECTOR;
	remaining = n + offset;

	equal = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(
		_mm256_load_si256((const __m256i *)(const void *)p), needle));
	equal &= 0xffffffffu << offset;

	while (remaining > PLATFORM_MEM_VECTOR) {
		if (equal != 0)
			goto found;
		p += PLATFORM_MEM_VECTOR;
		remaining -= PLATFORM_MEM_VECTOR;
		equal = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(
			_mm256_load_si256((const __m256i *)(const void *)p), needle));
	}
	equal &= platform_memchr_low_bytes(remaining);
	if (equal != 0)
		goto found;
	_mm256_zeroupper();
	return (NULL);

found:
	_mm256_zeroupper();
	return ((void *)(p + (unsigned)__builtin_ctz(equal)));
}
#endif

void *
_platform_memchr(const void *s, int c, size_t n)
{
	if (n != 0) {
		const unsigned char *p = s;

#if defined(__x86_64__)
		if (n >= PLATFORM_MEM_VECTOR && platform_has_avx2())
			return (_platform_memchr_avx2(s, c, n));
#endif
		do {
			if (*p++ == (unsigned char)c)
				return ((void *)(p - 1));
		} while (--n != 0);
	}
	return (NULL);
}

#if VARIANT_STATIC
void *
memchr(const void *s, int c, size_t n)
{
	return _platform_memchr(s, c, n);
}
#endif

#endif
