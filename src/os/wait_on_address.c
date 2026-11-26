/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ulock.h>

#define OS_SYNC_WAIT_ON_ADDRESS_SHARED 1U

int
os_sync_wait_on_address(void *addr, uint64_t value, size_t size, uint32_t flags)
{
	uint32_t op = (size == 8) ? UL_COMPARE_AND_WAIT64 : UL_COMPARE_AND_WAIT;
	if (flags & OS_SYNC_WAIT_ON_ADDRESS_SHARED) {
		op = (size == 8) ? UL_COMPARE_AND_WAIT64_SHARED : UL_COMPARE_AND_WAIT_SHARED;
	}
	int ret = __ulock_wait(op | ULF_NO_ERRNO, addr, value, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return ret;
}

int
os_sync_wake_by_address_any(void *addr, size_t size, uint32_t flags)
{
	uint32_t op = (size == 8) ? UL_COMPARE_AND_WAIT64 : UL_COMPARE_AND_WAIT;
	if (flags & OS_SYNC_WAIT_ON_ADDRESS_SHARED) {
		op = (size == 8) ? UL_COMPARE_AND_WAIT64_SHARED : UL_COMPARE_AND_WAIT_SHARED;
	}
	int ret = __ulock_wake(op | ULF_NO_ERRNO, addr, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return 0;
}

int
os_sync_wake_by_address_all(void *addr, size_t size, uint32_t flags)
{
	uint32_t op = (size == 8) ? UL_COMPARE_AND_WAIT64 : UL_COMPARE_AND_WAIT;
	if (flags & OS_SYNC_WAIT_ON_ADDRESS_SHARED) {
		op = (size == 8) ? UL_COMPARE_AND_WAIT64_SHARED : UL_COMPARE_AND_WAIT_SHARED;
	}
	int ret = __ulock_wake(op | ULF_WAKE_ALL | ULF_NO_ERRNO, addr, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return 0;
}
