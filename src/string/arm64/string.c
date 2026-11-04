/*
 * Copyright (c) 2026 Darling Team
 *
 * memmove, memset, memset_pattern4/8/16 and bzero for arm64.
 *
 * The generic versions are written for portability: memset stores four
 * bytes at a time through memmove, and like the rest of the tree they are
 * built unoptimized, so filling or copying a frame (megabytes) took
 * hundreds of milliseconds. These move 64 bytes a step with unaligned
 * 8-byte accesses, which arm64 allows on normal memory; the compiler turns
 * the steps into paired register loads and stores. This file is built
 * optimized and with -fno-builtin, so that its loops don't become calls to
 * the functions they implement.
 */

#include <platform/string.h>

#if defined(__arm64__)

typedef uint64_t __attribute__((aligned(1), may_alias)) unaligned64;

#define LOAD(p) (*(const unaligned64 *)(p))
#define STORE(p, v) (*(unaligned64 *)(p) = (v))

static inline void copy_forward(unsigned char *d, const unsigned char *s, size_t n)
{
	while (n >= 64) {
		uint64_t a = LOAD(s), b = LOAD(s + 8), c = LOAD(s + 16), e = LOAD(s + 24);
		uint64_t f = LOAD(s + 32), g = LOAD(s + 40), h = LOAD(s + 48), i = LOAD(s + 56);
		STORE(d, a); STORE(d + 8, b); STORE(d + 16, c); STORE(d + 24, e);
		STORE(d + 32, f); STORE(d + 40, g); STORE(d + 48, h); STORE(d + 56, i);
		s += 64;
		d += 64;
		n -= 64;
	}
	while (n >= 8) {
		STORE(d, LOAD(s));
		s += 8;
		d += 8;
		n -= 8;
	}
	while (n-- > 0)
		*d++ = *s++;
}

static inline void copy_backward(unsigned char *d, const unsigned char *s, size_t n)
{
	d += n;
	s += n;
	while (n >= 64) {
		s -= 64;
		d -= 64;
		n -= 64;
		uint64_t a = LOAD(s), b = LOAD(s + 8), c = LOAD(s + 16), e = LOAD(s + 24);
		uint64_t f = LOAD(s + 32), g = LOAD(s + 40), h = LOAD(s + 48), i = LOAD(s + 56);
		STORE(d, a); STORE(d + 8, b); STORE(d + 16, c); STORE(d + 24, e);
		STORE(d + 32, f); STORE(d + 40, g); STORE(d + 48, h); STORE(d + 56, i);
	}
	while (n >= 8) {
		s -= 8;
		d -= 8;
		n -= 8;
		STORE(d, LOAD(s));
	}
	while (n-- > 0)
		*--d = *--s;
}

void *
_platform_memmove(void *dst, const void *src, size_t length)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	if (length == 0 || d == s)
		return dst;
	/* Forward unless the destination starts inside the source: each 64-byte
	   step reads all of its source before writing, so a destination below
	   the source copies forward safely. */
	if (d < s || d >= s + length)
		copy_forward(d, s, length);
	else
		copy_backward(d, s, length);
	return dst;
}

/* Fills with the 16 bytes p0, p1 over and over, from b on. */
static inline void fill16(unsigned char *b, uint64_t p0, uint64_t p1, size_t len)
{
	while (len >= 64) {
		STORE(b, p0); STORE(b + 8, p1); STORE(b + 16, p0); STORE(b + 24, p1);
		STORE(b + 32, p0); STORE(b + 40, p1); STORE(b + 48, p0); STORE(b + 56, p1);
		b += 64;
		len -= 64;
	}
	while (len >= 16) {
		STORE(b, p0);
		STORE(b + 8, p1);
		b += 16;
		len -= 16;
	}
	/* The rest is the start of the pattern (little-endian, as stored). */
	for (size_t i = 0; i < len; i++)
		b[i] = (unsigned char)(i < 8 ? p0 >> (8 * i) : p1 >> (8 * (i - 8)));
}

void *
_platform_memset(void *b, int c, size_t len)
{
	uint64_t v = (unsigned char)c * 0x0101010101010101ull;
	fill16(b, v, v, len);
	return b;
}

void
_platform_bzero(void *s, size_t n)
{
	fill16(s, 0, 0, n);
}

void
_platform_memset_pattern4(void *b, const void *pattern4, size_t len)
{
	uint64_t v = *(const uint32_t __attribute__((aligned(1), may_alias)) *)pattern4;
	v |= v << 32;
	fill16(b, v, v, len);
}

void
_platform_memset_pattern8(void *b, const void *pattern8, size_t len)
{
	uint64_t v = LOAD(pattern8);
	fill16(b, v, v, len);
}

void
_platform_memset_pattern16(void *b, const void *pattern16, size_t len)
{
	const unsigned char *p = pattern16;
	fill16(b, LOAD(p), LOAD(p + 8), len);
}

#endif
