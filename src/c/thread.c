/**
 * @file thread.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kernel - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-OS/Kernel.
 *
 * Arctan is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @DESCRIPTION
*/
#include "arch/context.h"
#include "arch/pager.h"
#include "config.h"
#include "global.h"
#include "lib/atomics.h"
#include "lib/util.h"
#include "mm/allocator.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mp/scheduler.h"
#include "userspace/process.h"
#include <stdio.h>
#include "userspace/thread.h"

#ifdef ARC_TARGET_ARCH_X86_64
        #include "arch/x86-64/ctrl_regs.h"
#endif

static uint64_t tid_counter = 0;

ARC_Thread *thread_create(ARC_Process *process, void *entry, size_t stack_size) {
	if (process == NULL || entry == NULL || stack_size == 0) {
		ARC_DEBUG(ERR, "Failed to create thread, improper parameters (%p %lu)\n", entry, stack_size);
		return NULL;
	}

	ARC_Thread *thread = (struct ARC_Thread *)alloc(sizeof(*thread));

	if (thread == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate thread\n");
		return NULL;
	}

	memset(thread, 0, sizeof(*thread));
	init_static_spinlock(&thread->lock);

	if ((thread->context = init_context(1 << ARC_CONTEXT_FLAG_FLOATS, &thread->features)) == NULL) {
		ARC_DEBUG(ERR, "Failed to initialize context\n");
		goto clean_up;
	}

	thread->stack.size = stack_size;

	if ((thread->stack.phys = pmm_alloc(stack_size)) == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate physical memory for thread\n");
		goto clean_up;
	}

	if ((thread->stack.virt = (void *)vmm_alloc(process->allocator, stack_size)) == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate virtual memory for thread\n");
		goto clean_up;
	}

	if (pager_map(process->page_tables.user, (uintptr_t)thread->stack.virt, ARC_HHDM_TO_PHYS(thread->stack.phys), stack_size,
		      (1 << ARC_PAGER_RW) | (1 << ARC_PAGER_NX) | (process->userspace << ARC_PAGER_US)) != 0) {
		ARC_DEBUG(ERR, "Failed to map memory for thread\n");
		goto clean_up;
	}

	void *stack = thread->stack.virt + thread->stack.size - 16;
	context_setup_for_thread(thread->context, entry, stack, process->page_tables.user, process->userspace);

	thread->state = ARC_THREAD_READY;
	thread->tid = ARC_ATOMIC_INC(tid_counter);

	if (process_associate_thread(process, thread) != 0) {
		ARC_DEBUG(ERR, "Failed to associate thread with process\n");
		pager_unmap(process->page_tables.user, (uintptr_t)thread->stack.virt, stack_size, NULL);
		goto clean_up;
	}

	ARC_DEBUG(INFO, "Created thread %lu (%p)\n", thread->tid, thread);

	return thread;

	clean_up:;
	if (thread->context != NULL) {
		uninit_context(thread->context);
	}

	if (thread->stack.phys != NULL) {
		pmm_free(thread->stack.phys);
	}

	if (thread->stack.virt != NULL) {
		vmm_free(process->allocator, thread->stack.virt);
	}

	free(thread);

	return NULL;
}

int thread_delete(ARC_Thread *thread) {
	if (thread == NULL) {
		ARC_DEBUG(ERR, "Failed to delete thread, given thread is NULL\n");
		return -1;
	}

	// TODO: Signal an exit to the thread?

	spinlock_lock(&thread->lock);

	free(thread);

	return 0;
}
