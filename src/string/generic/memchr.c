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

/* Shorter ranges are scanned faster byte by byte than set up for the
   vector scan. */
#define PLATFORM_MEMCHR_SHORT	12

/* The bits of the first count bytes of a 32-byte block, count <= 32. */
static inline unsigned
platform_memchr_low_bytes(size_t count)
{
	return (count >= PLATFORM_MEM_VECTOR ? 0xffffffffu : (1u << count) - 1);
}

/*
 * Compares the 32-byte aligned blocks starting at *block with c, at most
 * blocks of them (blocks > 0), and stops at the first block holding c.
 * Returns that block's match bits, with *block pointing at it, or 0 with
 * *block pointing at the last block compared. Written out in assembly so
 * that its speed does not depend on the optimisation level the library is
 * built with.
 */
static inline unsigned
platform_memchr_blocks(const unsigned char **block, int c, size_t blocks)
{
	const unsigned char *p = *block;
	unsigned equal;

	__asm__ volatile(
		"vmovd %k[value], %%xmm0\n\t"
		"vpbroadcastb %%xmm0, %%ymm0\n\t"
		"jmp 2f\n"
		"1:\n\t"
		"addq $32, %[p]\n"
		"2:\n\t"
		"vpcmpeqb (%[p]), %%ymm0, %%ymm1\n\t"
		"vpmovmskb %%ymm1, %[equal]\n\t"
		"testl %[equal], %[equal]\n\t"
		"jnz 3f\n\t"
		"decq %[blocks]\n\t"
		"jnz 1b\n"
		"3:\n\t"
		"vzeroupper"
		: [p] "+r"(p), [blocks] "+r"(blocks), [equal] "=&r"(equal)
		: [value] "r"(c)
		: "xmm0", "xmm1", "memory", "cc");
	*block = p;
	return (equal);
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
__attribute__((visibility("hidden")))
void *
_platform_memchr_avx2(const void *s, int c, size_t n)
{
	const size_t offset = (uintptr_t)s & (PLATFORM_MEM_VECTOR - 1);
	const unsigned char *p = (const unsigned char *)s - offset;
	size_t remaining;
	unsigned equal;

	/* Bytes from p to the end of the range. A length that would wrap
	   cannot be reached by the scan anyway. */
	if (n > SIZE_MAX - PLATFORM_MEM_VECTOR)
		n = SIZE_MAX - PLATFORM_MEM_VECTOR;
	remaining = n + offset;

	/* The first block, without the bytes before s. */
	equal = platform_memchr_blocks(&p, c, 1) & (0xffffffffu << offset);
	if (remaining <= PLATFORM_MEM_VECTOR)
		goto last;
	if (equal != 0)
		goto found;
	p += PLATFORM_MEM_VECTOR;
	remaining -= PLATFORM_MEM_VECTOR;

	/* The blocks before the last one are whole. */
	if (remaining > PLATFORM_MEM_VECTOR) {
		const size_t whole = (remaining - 1) / PLATFORM_MEM_VECTOR;
		const unsigned char *start = p;

		equal = platform_memchr_blocks(&p, c, whole);
		if (equal != 0)
			goto found;
		p += PLATFORM_MEM_VECTOR;
		remaining -= (size_t)(p - start);
	}

	/* The last block, without the bytes after s+n. */
	equal = platform_memchr_blocks(&p, c, 1);
last:
	equal &= platform_memchr_low_bytes(remaining);
	if (equal == 0)
		return (NULL);
found:
	return ((void *)(p + (unsigned)__builtin_ctz(equal)));
}
#endif

void *
_platform_memchr(const void *s, int c, size_t n)
{
	if (n != 0) {
		const unsigned char *p = s;

#if defined(__x86_64__)
		if (n >= PLATFORM_MEMCHR_SHORT && platform_has_avx2())
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
