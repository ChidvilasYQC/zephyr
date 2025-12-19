/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#define TEST_THREAD_STACKSIZE 2048

volatile bool expect_fault;

#define TEST_THREAD_CREATOR_PRIORITY (CONFIG_NUM_PREEMPT_PRIORITIES - 1)
#define TEST_THREAD_REUSED_PRIORITY  (CONFIG_NUM_PREEMPT_PRIORITIES - 2)

struct k_thread test_thread_creator;
struct k_thread test_thread_reused;

K_SEM_DEFINE(test_sem, 0, 1);

static K_KERNEL_STACK_DEFINE(test_thread_creator_stack, TEST_THREAD_STACKSIZE);
static K_KERNEL_STACK_DEFINE(test_thread_reused_stack, TEST_THREAD_STACKSIZE);

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *pEsf)
{
	printk("Caught system error -- reason %d\n", reason);

	if (expect_fault && reason == K_ERR_KERNEL_PANIC) {
		expect_fault = false;
		ztest_test_pass();
	} else {
		printk("Unexpected fault during test\n");
		TC_END_REPORT(TC_FAIL);
		k_fatal_halt(reason);
	}
}

void reused_thread_waitforever(void *p1, void *p2, void *p3)
{
	printf("In wait forver reused thread\n");
	k_sem_take(&test_sem, K_FOREVER);
	ztest_test_fail();
}

void reused_thread_return(void *p1, void *p2, void *p3)
{
	printf("In immediate return reused thread\n");
}

static void creator_thread(void *p1, void *p2, void *p3)
{
	k_tid_t tid1, tid2;

	/* Test reusing TID after thread join */
	tid1 = k_thread_create(&test_thread_reused, test_thread_reused_stack,
			       K_KERNEL_STACK_SIZEOF(test_thread_reused_stack),
			       reused_thread_return, NULL, NULL, NULL,
			       TEST_THREAD_REUSED_PRIORITY, 0, K_NO_WAIT);
	k_thread_join(&test_thread_reused, K_FOREVER);

	/* Try to reuse the same thread structure */
	tid2 = k_thread_create(&test_thread_reused, test_thread_reused_stack,
			       K_KERNEL_STACK_SIZEOF(test_thread_reused_stack),
			       reused_thread_return, NULL, NULL, NULL,
			       TEST_THREAD_REUSED_PRIORITY, 0, K_NO_WAIT);
	k_thread_join(&test_thread_reused, K_FOREVER);

	/* Test reusing TID before thread join */
	tid1 = k_thread_create(&test_thread_reused, test_thread_reused_stack,
			       K_KERNEL_STACK_SIZEOF(test_thread_reused_stack),
			       reused_thread_waitforever, NULL, NULL, NULL,
			       TEST_THREAD_REUSED_PRIORITY, 0, K_NO_WAIT);

	expect_fault = true;

	/* Try to reuse the same thread structure */
	tid2 = k_thread_create(&test_thread_reused, test_thread_reused_stack,
			       K_KERNEL_STACK_SIZEOF(test_thread_reused_stack),
			       reused_thread_waitforever, NULL, NULL, NULL,
			       TEST_THREAD_REUSED_PRIORITY, 0, K_NO_WAIT);

	printf("Giving test sem\n");
	k_sem_give(&test_sem);
	printf("Giving test sem again\n");
	k_sem_give(&test_sem);

	k_thread_join(tid1, K_FOREVER);

	ztest_test_fail();
}

ZTEST(reuse_tid, test_tid_reuse)
{
	k_tid_t tid;

	tid = k_thread_create(&test_thread_creator, test_thread_creator_stack,
			      K_KERNEL_STACK_SIZEOF(test_thread_creator_stack), creator_thread,
			      NULL, NULL, NULL, TEST_THREAD_CREATOR_PRIORITY, 0, K_NO_WAIT);

	k_thread_join(tid, K_FOREVER);

	ztest_test_fail();
}

ZTEST_SUITE(reuse_tid, NULL, NULL, NULL, NULL, NULL);
