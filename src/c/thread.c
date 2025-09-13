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
#include "arch/floats.h"
#include "arch/pager.h"
#include "global.h"
#include "lib/util.h"
#include "mm/allocator.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mp/scheduler.h"
#include "userspace/process.h"
#include "userspace/thread.h"

#ifdef ARC_TARGET_ARCH_X86_64
#include "arch/x86-64/ctrl_regs.h"
#endif

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

	if (init_floats(&thread->context, 0) != 0) {
		ARC_DEBUG(ERR, "Failed to initialize floats for thread\n");
		goto clean_up;
	}

	if ((thread->pstack = pmm_alloc(stack_size)) == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate physical memory for thread\n");
		goto clean_up;
	}

	if ((thread->vstack = (void *)vmm_alloc(process->allocator, stack_size)) == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate virtual memory for thread\n");
		goto clean_up;
	}

	if (pager_map(process->page_tables, (uintptr_t)thread->vstack, ARC_HHDM_TO_PHYS(thread->pstack), stack_size,
		      (1 << ARC_PAGER_RW) | (1 << ARC_PAGER_NX) | (1 << ARC_PAGER_US)) != 0) {
		ARC_DEBUG(ERR, "Failed to map memory for thread\n");
		goto clean_up;
	}

	thread->state = ARC_THREAD_READY;
	thread->stack_size = stack_size;

#ifdef ARC_TARGET_ARCH_X86_64
		thread->context.frame.rip = (uintptr_t)entry;
		thread->context.frame.cs = process->page_tables == (void *)ARC_PHYS_TO_HHDM(Arc_KernelPageTables) ? 0x8 : 0x23;
		thread->context.frame.ss = process->page_tables == (void *)ARC_PHYS_TO_HHDM(Arc_KernelPageTables) ? 0x10 : 0x1b;
		thread->context.frame.gpr.rbp = (uintptr_t)thread->vstack + stack_size - 16;
		thread->context.frame.rsp = thread->context.frame.gpr.rbp;
		thread->context.frame.rflags = (1 << 9) | (1 << 1) | (0b11 << 12);
		thread->context.cr0 = _x86_getCR0();
		thread->context.cr4 = _x86_getCR4();
#endif

	if (process_associate_thread(process, thread) != 0) {
		ARC_DEBUG(ERR, "Failed to associate thread with process\n");
		pager_unmap(process->page_tables, (uintptr_t)thread->vstack, stack_size, NULL);
		goto clean_up;
	}

	ARC_DEBUG(INFO, "Created thread (%p)\n", entry);

	return thread;

	clean_up:;
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
