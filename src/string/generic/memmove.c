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

#if !_PLATFORM_OPTIMIZED_MEMMOVE

#if defined(__x86_64__)
#include <platform/string_avx2.h>
#endif

#include <sys/types.h>

/*
 * sizeof(word) MUST BE A POWER OF TWO
 * SO THAT wmask BELOW IS ALL ONES
 */
typedef	long word;		/* "word" used for optimal copy speed */

#define	wsize	sizeof(word)
#define	wmask	(wsize - 1)

#if defined(__x86_64__)

/*
 * A forward copy moves low to high and reads each 32-byte block before it
 * writes it, so an overlapping copy stays correct: the block written at
 * offset i ends below the first source byte not read yet. The block that does
 * not fill a vector is finished with the 8, 4, 2 and 1 byte moves of
 * platform_copy_short, which read every byte before they write any of it.
 */
__attribute__((target("avx2"), visibility("hidden")))
static void
platform_copy_forward_avx2(unsigned char *destination, const unsigned char *source, size_t length)
{
	size_t copied = 0;

	while (length - copied >= PLATFORM_MEM_STEP) {
		const __m256i first = _mm256_loadu_si256((const __m256i *)(const void *)(source + copied));
		const __m256i second = _mm256_loadu_si256((const __m256i *)(const void *)(source + copied + PLATFORM_MEM_VECTOR));

		_mm256_storeu_si256((__m256i *)(void *)(destination + copied), first);
		_mm256_storeu_si256((__m256i *)(void *)(destination + copied + PLATFORM_MEM_VECTOR), second);
		copied += PLATFORM_MEM_STEP;
	}
	while (length - copied >= PLATFORM_MEM_VECTOR) {
		const __m256i block = _mm256_loadu_si256((const __m256i *)(const void *)(source + copied));

		_mm256_storeu_si256((__m256i *)(void *)(destination + copied), block);
		copied += PLATFORM_MEM_VECTOR;
	}
	platform_copy_short(destination + copied, source + copied, length - copied);
}

/* The same, high to low, for a destination above the source. */
__attribute__((target("avx2"), visibility("hidden")))
static void
platform_copy_backward_avx2(unsigned char *destination, const unsigned char *source, size_t length)
{
	size_t copied = 0;

	while (length - copied >= PLATFORM_MEM_STEP) {
		const __m256i first = _mm256_loadu_si256((const __m256i *)(const void *)(source + length - copied - PLATFORM_MEM_STEP));
		const __m256i second = _mm256_loadu_si256((const __m256i *)(const void *)(source + length - copied - PLATFORM_MEM_VECTOR));

		copied += PLATFORM_MEM_STEP;
		_mm256_storeu_si256((__m256i *)(void *)(destination + length - copied), first);
		_mm256_storeu_si256((__m256i *)(void *)(destination + length - copied + PLATFORM_MEM_VECTOR), second);
	}
	while (length - copied >= PLATFORM_MEM_VECTOR) {
		__m256i block;

		copied += PLATFORM_MEM_VECTOR;
		block = _mm256_loadu_si256((const __m256i *)(const void *)(source + length - copied));
		_mm256_storeu_si256((__m256i *)(void *)(destination + length - copied), block);
	}
	platform_copy_short(destination, source, length - copied);
}

__attribute__((target("avx2"), visibility("hidden")))
void *
_platform_memmove_avx2(void *dst, const void *src, size_t length)
{
	unsigned char *destination = (unsigned char *)dst;
	const unsigned char *source = (const unsigned char *)src;
	const unsigned long destinationAddress = (unsigned long)destination;
	const unsigned long sourceAddress = (unsigned long)source;

	if (destinationAddress < sourceAddress) {
		platform_copy_forward_avx2(destination, source, length);
	} else if (destinationAddress > sourceAddress) {
		platform_copy_backward_avx2(destination, source, length);
	}
	_mm256_zeroupper();
	return (dst);
}

#endif /* __x86_64__ */

/*
 * Copy a block of memory, handling overlap.
 * This is the routine that actually implements
 * (the portable versions of) bcopy, memcpy, and memmove.
 */

void *
_platform_memmove(void *dst0, const void *src0, size_t length)
{
	char *dst = dst0;
	const char *src = src0;
	size_t t;

#if defined(__x86_64__)
	if (length >= PLATFORM_MEM_VECTOR && platform_has_avx2())
		return (_platform_memmove_avx2(dst0, src0, length));
#endif

	if (length == 0 || dst == src)		/* nothing to do */
		goto done;

	/*
	 * Macros: loop-t-times; and loop-t-times, t>0
	 */
#define	TLOOP(s) if (t) TLOOP1(s)
#define	TLOOP1(s) do { s; } while (--t)

	if ((unsigned long)dst < (unsigned long)src) {
		/*
		 * Copy forward.
		 */
		t = (uintptr_t)src;	/* only need low bits */
		if ((t | (uintptr_t)dst) & wmask) {
			/*
			 * Try to align operands.  This cannot be done
			 * unless the low bits match.
			 */
			if ((t ^ (uintptr_t)dst) & wmask || length < wsize)
				t = length;
			else
				t = wsize - (t & wmask);
			length -= t;
			TLOOP1(*dst++ = *src++);
		}
		/*
		 * Copy whole words, then mop up any trailing bytes.
		 */
		t = length / wsize;
		TLOOP(*(word *)dst = *(word *)src; src += wsize; dst += wsize);
		t = length & wmask;
		TLOOP(*dst++ = *src++);
	} else {
		/*
		 * Copy backwards.  Otherwise essentially the same.
		 * Alignment works as before, except that it takes
		 * (t&wmask) bytes to align, not wsize-(t&wmask).
		 */
		src += length;
		dst += length;
		t = (uintptr_t)src;
		if ((t | (uintptr_t)dst) & wmask) {
			if ((t ^ (uintptr_t)dst) & wmask || length <= wsize)
				t = length;
			else
				t &= wmask;
			length -= t;
			TLOOP1(*--dst = *--src);
		}
		t = length / wsize;
		TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(word *)src);
		t = length & wmask;
		TLOOP(*--dst = *--src);
	}
done:
	return (dst0);
}

#if VARIANT_STATIC
void *
memmove(void *dst0, const void *src0, size_t length)
{
	return _platform_memmove(dst0, src0, length);
}

void *
memcpy(void *dst0, const void *src0, size_t length)
{
	return _platform_memmove(dst0, src0, length);
}
#endif

#endif
