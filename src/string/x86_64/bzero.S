/*
 * QUIRK: on x86_64, bzero() and __bzero() return with the destination
 * pointer in RAX, although they are declared void. Not documented.
 * Evidence: black-box observation on macOS 27.0.1 (x86_64 under Rosetta,
 * tier 2): RAX holds the destination after both calls. The original
 * report was a Perl 5.28 build whose perl_alloc() used RAX after a
 * __bzero call; clang 21 was not seen emitting such code.
 * Test: darling_bzero_return_regress.c.
 */

	.text
	.align 4, 0x00

	.globl __platform_bzero
__platform_bzero:
	pushq	%rdi
	movq	%rsi, %rdx
	xorl	%esi, %esi
	call	__platform_memset
	popq	%rax
	ret
