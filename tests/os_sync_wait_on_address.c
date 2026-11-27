#include <errno.h>
#include <os/os_sync_wait_on_address.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL: %s (line %d, errno=%d)\n", #c, __LINE__, errno); return 1; } } while (0)

struct waiter_ctx {
	_Atomic uint32_t *word;
	_Atomic int waiting;
	_Atomic int woken;
	uint32_t expected;
};

static void *
waiter_fn(void *arg)
{
	struct waiter_ctx *ctx = arg;
	atomic_fetch_add(&ctx->waiting, 1);
	while (atomic_load(ctx->word) == ctx->expected) {
		int rc = os_sync_wait_on_address((void *)ctx->word, ctx->expected,
				sizeof(uint32_t), OS_SYNC_WAIT_ON_ADDRESS_NONE);
		if (rc < 0 && errno != EINTR) {
			return (void *)(intptr_t)1;
		}
	}
	atomic_fetch_add(&ctx->woken, 1);
	return NULL;
}

int
main(void)
{
	uint32_t val32 = 42;
	uint64_t val64 = (1ULL << 32) | 42U;
	uint64_t misalign_buf[2] = {0, 0};
	void *misalign4 = (char *)&misalign_buf[0] + 2;
	void *misalign8 = (char *)&misalign_buf[0] + 4;

	/* Value mismatch returns >= 0 (including 64-bit high-half mismatch). */
	CHECK(os_sync_wait_on_address(&val32, 0, sizeof(val32), OS_SYNC_WAIT_ON_ADDRESS_NONE) >= 0);
	CHECK(os_sync_wait_on_address(&val64, 42U, sizeof(val64), OS_SYNC_WAIT_ON_ADDRESS_NONE) >= 0);

	/* Validation errors return -1 with errno == EINVAL. */
	CHECK(os_sync_wait_on_address(NULL, 0, 4, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wait_on_address(&val32, 42, 2, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wait_on_address(&val32, 42, 4, 0x2) == -1 && errno == EINVAL);
	CHECK(os_sync_wait_on_address(misalign4, 0, 4, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wait_on_address(misalign8, 0, 8, 0) == -1 && errno == EINVAL);

	CHECK(os_sync_wake_by_address_any(NULL, 4, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_any(&val32, 2, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_any(&val32, 4, 0x2) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_any(misalign4, 4, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_any(misalign8, 8, 0) == -1 && errno == EINVAL);

	CHECK(os_sync_wake_by_address_all(NULL, 4, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_all(&val32, 2, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_all(&val32, 4, 0x2) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_all(misalign4, 4, 0) == -1 && errno == EINVAL);
	CHECK(os_sync_wake_by_address_all(misalign8, 8, 0) == -1 && errno == EINVAL);

	/* Zero waiters returns -1 with errno == ENOENT. */
	CHECK(os_sync_wake_by_address_any(&val32, sizeof(val32), OS_SYNC_WAKE_BY_ADDRESS_NONE) == -1 && errno == ENOENT);
	CHECK(os_sync_wake_by_address_all(&val64, sizeof(val64), OS_SYNC_WAKE_BY_ADDRESS_SHARED) == -1 && errno == ENOENT);

	/* Single waiter + wake_any. */
	_Atomic uint32_t word = 0;
	struct waiter_ctx ctx1 = { .word = &word, .waiting = 0, .woken = 0, .expected = 0 };
	pthread_t t1;
	CHECK(pthread_create(&t1, NULL, waiter_fn, &ctx1) == 0);
	while (atomic_load(&ctx1.waiting) < 1) {
		usleep(1000);
	}
	usleep(10000);
	atomic_store(&word, 1);
	CHECK(os_sync_wake_by_address_any((void *)&word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE) == 0);
	void *r1 = NULL;
	CHECK(pthread_join(t1, &r1) == 0 && r1 == NULL);
	CHECK(atomic_load(&ctx1.woken) == 1);

	/* Two waiters + wake_all. */
	struct waiter_ctx ctx2 = { .word = &word, .waiting = 0, .woken = 0, .expected = 1 };
	pthread_t t2, t3;
	CHECK(pthread_create(&t2, NULL, waiter_fn, &ctx2) == 0);
	CHECK(pthread_create(&t3, NULL, waiter_fn, &ctx2) == 0);
	while (atomic_load(&ctx2.waiting) < 2) {
		usleep(1000);
	}
	usleep(10000);
	atomic_store(&word, 2);
	CHECK(os_sync_wake_by_address_all((void *)&word, sizeof(uint32_t), OS_SYNC_WAKE_BY_ADDRESS_NONE) == 0);
	void *r2 = NULL, *r3 = NULL;
	CHECK(pthread_join(t2, &r2) == 0 && r2 == NULL);
	CHECK(pthread_join(t3, &r3) == 0 && r3 == NULL);
	CHECK(atomic_load(&ctx2.woken) == 2);

	puts("PASS");
	return 0;
}
