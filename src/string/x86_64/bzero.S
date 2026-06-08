/*
 * Preserve the effective Darwin ___bzero return-register contract.
 *
 * LLVM may lower a value-producing memset(ptr, 0, len) to ___bzero on Darwin
 * even though bzero is declared void. Keep the original destination in RAX at
 * the actual return instruction so compiler-generated C epilogues cannot
 * clobber it.
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
