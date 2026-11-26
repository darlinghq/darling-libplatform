#ifndef __OS_SYNC_WAIT_ON_ADDRESS_H__
#define __OS_SYNC_WAIT_ON_ADDRESS_H__

#include <os/base.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

typedef uint32_t os_sync_wait_on_address_flags_t;
#define OS_SYNC_WAIT_ON_ADDRESS_NONE 0x00000000U
#define OS_SYNC_WAIT_ON_ADDRESS_SHARED 0x00000001U

typedef uint32_t os_sync_wake_by_address_flags_t;
#define OS_SYNC_WAKE_BY_ADDRESS_NONE 0x00000000U
#define OS_SYNC_WAKE_BY_ADDRESS_SHARED 0x00000001U

OS_EXPORT
int os_sync_wait_on_address(void *addr, uint64_t value, size_t size,
		os_sync_wait_on_address_flags_t flags);

OS_EXPORT
int os_sync_wake_by_address_any(void *addr, size_t size,
		os_sync_wake_by_address_flags_t flags);

OS_EXPORT
int os_sync_wake_by_address_all(void *addr, size_t size,
		os_sync_wake_by_address_flags_t flags);

__END_DECLS

#endif /* __OS_SYNC_WAIT_ON_ADDRESS_H__ */
