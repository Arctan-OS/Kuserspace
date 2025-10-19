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

	struct ARC_Thread *thread = (struct ARC_Thread *)alloc(sizeof(*thread));

	if (thread == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate thread\n");
		return NULL;
	}

	memset(thread, 0, sizeof(*thread));
	init_static_spinlock(&thread->lock);

	if ((thread->context = init_context(0)) == NULL) {
		ARC_DEBUG(ERR, "Failed to initialize context\n");
		goto clean_up;
	}

	thread->stack_size = stack_size;

	if ((thread->pstack = pmm_alloc(stack_size)) == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate physical memory for thread\n");
		goto clean_up;
	}

	if ((thread->vstack = (void *)vmm_alloc(process->allocator, stack_size)) == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate virtual memory for thread\n");
		goto clean_up;
	}

	if (pager_map(process->page_tables.user, (uintptr_t)thread->vstack, ARC_HHDM_TO_PHYS(thread->pstack), stack_size,
		      (1 << ARC_PAGER_RW) | (1 << ARC_PAGER_NX) | (process->userspace << ARC_PAGER_US)) != 0) {
		ARC_DEBUG(ERR, "Failed to map memory for thread\n");
		goto clean_up;
	}

//	if (pager_map(process->page_tables.user, (uintptr_t)thread->context, ARC_HHDM_TO_PHYS(thread->context), sizeof(*thread->context),
//                     (1 << ARC_PAGER_RW) | (1 << ARC_PAGER_NX)) != 0) {
//		ARC_DEBUG(ERR, "Failed to map in thread's context\n");
//		goto clean_up;
//	}

	// NOTE: I am conflicted about this bit of code here. I want to keep
	//       ifdefs out of the code as much as possible - so keeping code
	//       that would need ifdefs in the architecture specific sections,
	//       but a lot of this code is general only context prepartion is
	//       tied to a specific architecture
#ifdef ARC_TARGET_ARCH_X86_64
		thread->context->frame.rip = (uintptr_t)entry;
		thread->context->frame.cs = process->userspace ? 0x23 : 0x8;

		thread->context->frame.ss = process->userspace ? 0x1b : 0x10;
		thread->context->frame.gpr.rbp = (uintptr_t)thread->vstack + stack_size - 16;
		thread->context->frame.rsp = thread->context->frame.gpr.rbp;

		thread->context->frame.rflags = (1 << 9) | (1 << 1) | (0b11 << 12);

		thread->context->frame.gpr.cr0 = _x86_getCR0();
		thread->context->frame.gpr.cr3 = ARC_HHDM_TO_PHYS(process->page_tables.user);
		thread->context->frame.gpr.cr4 = _x86_getCR4();
#endif

	thread->state = ARC_THREAD_READY;
	thread->tid = ARC_ATOMIC_INC(tid_counter);

	if (process_associate_thread(process, thread) != 0) {
		ARC_DEBUG(ERR, "Failed to associate thread with process\n");
		pager_unmap(process->page_tables.user, (uintptr_t)thread->vstack, stack_size, NULL);
		goto clean_up;
	}

	ARC_DEBUG(INFO, "Created thread %lu (%p)\n", thread->tid, thread);

	return thread;

	clean_up:;
	if (thread->context != NULL) {
		uninit_context(thread->context);
	}

	if (thread->pstack != NULL) {
		pmm_free(thread->pstack);
	}

	if (thread->vstack != NULL) {
		vmm_free(process->allocator, thread->vstack);
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
