/*
 * Copyright (c) 2026 Darling Team
 *
 * The plain names for the static library (dyld links it), as the generic
 * versions give them in theirs. src/string/arm64/string.c is built into
 * the 64-bit objects without VARIANT_STATIC, so these live here.
 */

#include <platform/string.h>

#if defined(__arm64__) && VARIANT_STATIC

void *
memmove(void *dst, const void *src, size_t length)
{
	return _platform_memmove(dst, src, length);
}

void *
memcpy(void *dst, const void *src, size_t length)
{
	return _platform_memmove(dst, src, length);
}

void *
memset(void *b, int c, size_t len)
{
	return _platform_memset(b, c, len);
}

void
bzero(void *s, size_t n)
{
	_platform_bzero(s, n);
}

void
__bzero(void *s, size_t n)
{
	_platform_bzero(s, n);
}

void
memset_pattern4(void *b, const void *pattern4, size_t len)
{
	_platform_memset_pattern4(b, pattern4, len);
}

void
memset_pattern8(void *b, const void *pattern8, size_t len)
{
	_platform_memset_pattern8(b, pattern8, len);
}

void
memset_pattern16(void *b, const void *pattern16, size_t len)
{
	_platform_memset_pattern16(b, pattern16, len);
}

#endif
