/*
 * Copyright (c) 2026 Darling Developers. All rights reserved.
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

/*
 * AVX2 paths for the generic x86_64 string routines.
 *
 * No _PLATFORM_OPTIMIZED_* selector is set, so memmove, memset, bzero,
 * memcmp and memchr all run the portable loops in src/string/generic, which
 * move at most one word per iteration. The routines declared here add a
 * 32-byte wide path. They are chosen at run time from the commpage
 * capability word (the same word OSAtomicFifo.c reads), so a CPU without
 * AVX2, or without OS support for the AVX state, keeps the portable loops.
 * A capability word that is still zero also selects the portable loops.
 * Large non-overlapping copies and large fills use REP MOVSB/STOSB when the
 * CPU reports enhanced REP string support, with or without AVX2.
 *
 * The library may be built without optimisation, which turns vector
 * intrinsics into slow stack traffic, so the hot loops are inline assembly.
 *
 * None of these routines calls out of this library, so they stay
 * async-signal safe.
 */

#ifndef _PLATFORM_STRING_AVX2_H_
#define _PLATFORM_STRING_AVX2_H_

#include <platform/string.h>

#if defined(__x86_64__)

#include <stdint.h>
#include <immintrin.h>
#include <System/i386/cpu_capabilities.h>

void *_platform_memmove_avx2(void *dst, const void *src, size_t n);
void *_platform_memset_avx2(void *b, int c, size_t n);
int _platform_memcmp_avx2(const void *s1, const void *s2, size_t n);
void *_platform_memchr_avx2(const void *s, int c, size_t n);

/* The vector width, and the number of bytes one pass of the unrolled loops
   moves. */
#define PLATFORM_MEM_VECTOR	32
#define PLATFORM_MEM_STEP	64

/*
 * The capability word is read once and cached. Two threads may store the
 * same value concurrently, which is harmless. A zero word is read again on
 * the next call, so a caller that runs before the word is filled in keeps
 * using the portable loops until it is.
 */
static inline uint64_t
platform_cpu_capabilities(void)
{
	static volatile uint64_t capabilities;

	uint64_t caps = capabilities;

	if (__builtin_expect(caps == 0, 0)) {
		caps = *(const volatile uint64_t *)(uintptr_t)_COMM_PAGE_CPU_CAPABILITIES64;
		capabilities = caps;
	}
	return (caps);
}

static inline int
platform_has_avx2(void)
{
	return ((platform_cpu_capabilities() & kHasAVX2_0) != 0);
}

/*
 * With enhanced REP MOVSB/STOSB (kHasENFSTRG, CPUID leaf 7 EBX bit 9), the
 * string instructions move whole cache lines internally and beat a vector
 * loop on large blocks. Below this size their start-up cost dominates.
 */
#define PLATFORM_MEM_STRING_THRESHOLD	2048

static inline int
platform_has_fast_strings(void)
{
	return ((platform_cpu_capabilities() & kHasENFSTRG) != 0);
}

/* Copies length bytes upwards; the caller ensures the blocks do not
   overlap. The direction flag is clear on function entry (System V ABI). */
static inline void
platform_copy_rep_movsb(void *destination, const void *source, size_t length)
{
	__asm__ volatile("rep movsb"
	                 : "+D"(destination), "+S"(source), "+c"(length)
	                 :
	                 : "memory");
}

static inline void
platform_fill_rep_stosb(void *destination, unsigned char value, size_t length)
{
	__asm__ volatile("rep stosb"
	                 : "+D"(destination), "+c"(length)
	                 : "a"(value)
	                 : "memory");
}

typedef unsigned long long __attribute__((aligned(1), may_alias)) platform_u64u;
typedef unsigned int __attribute__((aligned(1), may_alias)) platform_u32u;
typedef unsigned short __attribute__((aligned(1), may_alias)) platform_u16u;

/*
 * Copies fewer than 32 bytes with 8, 4, 2 and 1 byte moves. All loads are
 * done before any store, so the head and tail moves may overlap each other
 * and the copy is correct whichever way the buffers overlap. Only
 * [source, source+length) is read and [destination, destination+length)
 * written.
 */
static inline void
platform_copy_short(unsigned char *destination, const unsigned char *source,
                    size_t length)
{
	if (length >= 16) {
		const unsigned long long first = *(const platform_u64u *)source;
		const unsigned long long second = *(const platform_u64u *)(source + 8);
		const unsigned long long low = *(const platform_u64u *)(source + length - 16);
		const unsigned long long high = *(const platform_u64u *)(source + length - 8);
		*(platform_u64u *)destination = first;
		*(platform_u64u *)(destination + 8) = second;
		*(platform_u64u *)(destination + length - 16) = low;
		*(platform_u64u *)(destination + length - 8) = high;
	} else if (length >= 8) {
		const unsigned long long first = *(const platform_u64u *)source;
		const unsigned long long last = *(const platform_u64u *)(source + length - 8);
		*(platform_u64u *)destination = first;
		*(platform_u64u *)(destination + length - 8) = last;
	} else if (length >= 4) {
		const unsigned first = *(const platform_u32u *)source;
		const unsigned last = *(const platform_u32u *)(source + length - 4);
		*(platform_u32u *)destination = first;
		*(platform_u32u *)(destination + length - 4) = last;
	} else if (length >= 2) {
		const unsigned short first = *(const platform_u16u *)source;
		const unsigned short last = *(const platform_u16u *)(source + length - 2);
		*(platform_u16u *)destination = first;
		*(platform_u16u *)(destination + length - 2) = last;
	} else if (length != 0) {
		destination[0] = source[0];
	}
}

/*
 * Fills fewer than 32 bytes with 8, 4, 2 and 1 byte stores. Overlapping
 * stores write the same value twice, which is harmless.
 */
static inline void
platform_fill_short(unsigned char *destination, unsigned char value, size_t length)
{
	const unsigned long long wide = 0x0101010101010101ull * value;

	if (length >= 16) {
		*(platform_u64u *)destination = wide;
		*(platform_u64u *)(destination + 8) = wide;
		*(platform_u64u *)(destination + length - 16) = wide;
		*(platform_u64u *)(destination + length - 8) = wide;
	} else if (length >= 8) {
		*(platform_u64u *)destination = wide;
		*(platform_u64u *)(destination + length - 8) = wide;
	} else if (length >= 4) {
		*(platform_u32u *)destination = (unsigned)wide;
		*(platform_u32u *)(destination + length - 4) = (unsigned)wide;
	} else if (length >= 2) {
		*(platform_u16u *)destination = (unsigned short)wide;
		*(platform_u16u *)(destination + length - 2) = (unsigned short)wide;
	} else if (length != 0) {
		destination[0] = value;
	}
}

#endif /* __x86_64__ */

#endif /* _PLATFORM_STRING_AVX2_H_ */
