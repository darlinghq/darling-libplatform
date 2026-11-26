#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ulock.h>

#include "os/os_sync_wait_on_address.h"

static bool
ulock_op(void *addr, size_t size, uint32_t flags, uint32_t *op)
{
	if (addr == NULL || (size != 4 && size != 8) ||
			((uintptr_t)addr & (size - 1)) != 0 ||
			(flags & ~OS_SYNC_WAIT_ON_ADDRESS_SHARED) != 0) {
		errno = EINVAL;
		return false;
	}
	if (flags & OS_SYNC_WAIT_ON_ADDRESS_SHARED) {
		*op = (size == 8) ? UL_COMPARE_AND_WAIT64_SHARED : UL_COMPARE_AND_WAIT_SHARED;
	} else {
		*op = (size == 8) ? UL_COMPARE_AND_WAIT64 : UL_COMPARE_AND_WAIT;
	}
	return true;
}

int
os_sync_wait_on_address(void *addr, uint64_t value, size_t size,
		os_sync_wait_on_address_flags_t flags)
{
	uint32_t op;
	if (!ulock_op(addr, size, flags, &op)) {
		return -1;
	}
	int ret = __ulock_wait(op | ULF_NO_ERRNO, addr, value, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return ret;
}

static int
wake_by_address(void *addr, size_t size, os_sync_wake_by_address_flags_t flags,
		uint32_t extra_op)
{
	uint32_t op;
	if (!ulock_op(addr, size, flags, &op)) {
		return -1;
	}
	int ret = __ulock_wake(op | extra_op | ULF_NO_ERRNO, addr, 0);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}
	return 0;
}

int
os_sync_wake_by_address_any(void *addr, size_t size,
		os_sync_wake_by_address_flags_t flags)
{
	return wake_by_address(addr, size, flags, 0);
}

int
os_sync_wake_by_address_all(void *addr, size_t size,
		os_sync_wake_by_address_flags_t flags)
{
	return wake_by_address(addr, size, flags, ULF_WAKE_ALL);
}
