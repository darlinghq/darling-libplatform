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

#if !_PLATFORM_OPTIMIZED_MEMCMP

#if defined(__x86_64__)
#include <platform/string_avx2.h>

/*
 * Comparing one byte per iteration is slow even for short buffers. Blocks
 * are compared with SSE2 (always present on x86_64), or AVX2 when the CPU
 * has it, and the remainder with overlapping unaligned loads. No read goes
 * past the end of either buffer.
 */

static inline int
platform_memcmp16(const unsigned char *p1, const unsigned char *p2)
{
	const __m128i a = _mm_loadu_si128((const __m128i *)p1);
	const __m128i b = _mm_loadu_si128((const __m128i *)p2);
	const unsigned equal = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(a, b));

	if (equal != 0xffffu) {
		const unsigned i = (unsigned)__builtin_ctz(~equal);
		return (p1[i] - p2[i]);
	}
	return (0);
}

__attribute__((target("avx2")))
static inline int
platform_memcmp32(const unsigned char *p1, const unsigned char *p2)
{
	const __m256i a = _mm256_loadu_si256((const __m256i *)(const void *)p1);
	const __m256i b = _mm256_loadu_si256((const __m256i *)(const void *)p2);
	const unsigned equal = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, b));

	if (equal != 0xffffffffu) {
		const unsigned i = (unsigned)__builtin_ctz(~equal);
		return (p1[i] - p2[i]);
	}
	return (0);
}

/*
 * 32 bytes at a time, then the last possibly overlapping 32 bytes. Only
 * [s1, s1+n) and [s2, s2+n) are read, and a block is only reached when every
 * byte below it compared equal, so the returned difference is the first one.
 */
__attribute__((target("avx2"), visibility("hidden")))
int
_platform_memcmp_avx2(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1, *p2 = s2;
	int result;

	while (n >= PLATFORM_MEM_VECTOR) {
		if ((result = platform_memcmp32(p1, p2)) != 0) {
			_mm256_zeroupper();
			return (result);
		}
		p1 += PLATFORM_MEM_VECTOR; p2 += PLATFORM_MEM_VECTOR;
		n -= PLATFORM_MEM_VECTOR;
	}
	if (n != 0)
		result = platform_memcmp32(p1 + n - PLATFORM_MEM_VECTOR, p2 + n - PLATFORM_MEM_VECTOR);
	else
		result = 0;
	_mm256_zeroupper();
	return (result);
}

int
_platform_memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *p1 = s1, *p2 = s2;

	if (n >= PLATFORM_MEM_VECTOR && platform_has_avx2())
		return (_platform_memcmp_avx2(s1, s2, n));
	if (n >= 16) {
		int r;

		do {
			if ((r = platform_memcmp16(p1, p2)) != 0)
				return (r);
			p1 += 16; p2 += 16; n -= 16;
		} while (n >= 16);
		// the last (possibly overlapping) 16 bytes
		return (n != 0 ? platform_memcmp16(p1 + n - 16, p2 + n - 16) : 0);
	}
	if (n >= 8) {
		unsigned long long x = *(const platform_u64u *)p1 ^ *(const platform_u64u *)p2;
		unsigned i;

		if (x == 0) {
			p1 += n - 8; p2 += n - 8;
			x = *(const platform_u64u *)p1 ^ *(const platform_u64u *)p2;
			if (x == 0)
				return (0);
		}
		i = (unsigned)__builtin_ctzll(x) >> 3;
		return (p1[i] - p2[i]);
	}
	if (n >= 4) {
		unsigned x = *(const platform_u32u *)p1 ^ *(const platform_u32u *)p2;
		unsigned i;

		if (x == 0) {
			p1 += n - 4; p2 += n - 4;
			x = *(const platform_u32u *)p1 ^ *(const platform_u32u *)p2;
			if (x == 0)
				return (0);
		}
		i = (unsigned)__builtin_ctz(x) >> 3;
		return (p1[i] - p2[i]);
	}
	while (n != 0) {
		if (*p1 != *p2)
			return (*p1 - *p2);
		++p1; ++p2; --n;
	}
	return (0);
}
#else
int
_platform_memcmp(const void *s1, const void *s2, size_t n)
{
	if (n != 0) {
		const unsigned char *p1 = s1, *p2 = s2;

		do {
			if (*p1++ != *p2++)
				return (*--p1 - *--p2);
		} while (--n != 0);
	}
	return (0);
}

#endif

#if VARIANT_STATIC
int
memcmp(const void *s1, const void *s2, size_t n)
{
	return _platform_memcmp(s1, s2, n);
}

int
bcmp(const void *s1, const void *s2, size_t n)
{
	return _platform_memcmp(s1, s2, n);
}
#endif

#endif
