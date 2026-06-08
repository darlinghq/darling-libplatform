/*
 * Regression test for code generated from `return memset(p, 0, n)`.
 *
 * Clang may lower that expression to a tail call to ___bzero. Darwin's
 * implementation leaves the original destination pointer in the return
 * register even though bzero is declared as returning void. Perl 5.28's
 * perl_alloc() depends on this generated-code behavior in Darling builds.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

extern void *__darling_bzero_return(void *, size_t) asm("___bzero");

int
main(void)
{
	size_t size = 0xc50;
	unsigned char *ptr = malloc(size);
	void *ret;

	if (ptr == NULL) {
		perror("malloc");
		return 2;
	}

	for (size_t i = 0; i < size; ++i)
		ptr[i] = 0xa5;

	ret = __darling_bzero_return(ptr, size);
	if (ret != ptr) {
		fprintf(stderr, "___bzero returned %p for %p\n", ret, ptr);
		free(ptr);
		return 1;
	}

	for (size_t i = 0; i < size; ++i) {
		if (ptr[i] != 0) {
			fprintf(stderr, "___bzero left byte %zu as 0x%02x\n", i, ptr[i]);
			free(ptr);
			return 1;
		}
	}

	free(ptr);
	return 0;
}
