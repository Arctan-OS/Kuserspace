/**
 * @file process.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan.
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
#include <userspace/thread.h>
#include <lib/atomics.h>
#include <mm/vmm.h>
#include <userspace/process.h>
#include <mm/allocator.h>
#include <global.h>
#include <lib/util.h>
#include <userspace/loaders/elf.h>
#include <lib/perms.h>
#include <arch/smp.h>
#include <arch/x86-64/ctrl_regs.h>
#include <mm/pmm.h>
#include <arch/pager.h>
#include <lib/convention/sysv.h>

#define DEFAULT_MEMSIZE 0x1000 * 4096
#define DEFAULT_STACKSIZE 0x2000

struct ARC_Process *process_create_from_file(int userspace, char *filepath) {
	if (filepath == NULL) {
		ARC_DEBUG(ERR, "Failed to create process, no file given\n");
		return NULL;
	}

	struct ARC_File *file = NULL;

	if (vfs_open(filepath, 0, ARC_STD_PERM, &file) != 0) {
		ARC_DEBUG(ERR, "Failed to create process, failed to open file\n");
		return NULL;
	}

	struct ARC_Process *process = process_create(userspace, NULL);

	if (process == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate process\n");
		return  NULL;
	}

	struct ARC_ELFMeta *meta = load_elf(process->page_tables, file);

	void *base = (void *)0x1000; // TODO: Determine this from ELF meta

	process->allocator = init_vmm(base, DEFAULT_MEMSIZE);

	if (process->allocator == NULL) {
		// TODO: Destroy page maps, destroy currently allocated meta data
		//       free all memory mapped by ELF loader
		ARC_DEBUG(ERR, "Failed to create process allocator\n");
		return NULL;
	}
	
	struct ARC_Thread *main = thread_create(process->allocator, process->page_tables, meta->entry, DEFAULT_STACKSIZE);

	if (main == NULL) {
		process_delete(process);
		ARC_DEBUG(ERR, "Failed to create main thread\n");
		return NULL;
	}

	process_associate_thread(process, main);

	char *argv[] = {"hello", "world"};
	
	// TODO: Make architecture independent
	main->ctx.rsp -= sysv_prepare_entry_stack((uint64_t *)main->pstack, meta, NULL, 0, argv, 2);

	free(meta);

	ARC_DEBUG(INFO, "Created process from file %s\n", filepath);

	return process;
}

// Expected that process->base will be set by the caller
struct ARC_Process *process_create(int userspace, void *page_tables) {
	struct ARC_Process *process = (struct ARC_Process *)alloc(sizeof(*process));

	if (process == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate process\n");
		return  NULL;
	}

	if (page_tables == NULL) {
		if (userspace == 0) {
			// Not a userspace process
			page_tables = (void *)ARC_PHYS_TO_HHDM(Arc_KernelPageTables);
			goto skip_page_tables;
		}

		page_tables = pager_create_page_tables();

		if (page_tables == NULL) {
			ARC_DEBUG(ERR, "Failed to allocate page tables\n");
			free(process);
			return NULL;
		}

		pager_clone(page_tables, (uintptr_t)&__KERNEL_START__, (uintptr_t)&__KERNEL_START__,
			    ((uintptr_t)&__KERNEL_END__ - (uintptr_t)&__KERNEL_START__), 0);
	}

	skip_page_tables:;

	memset(process, 0, sizeof(*process));

	process->page_tables = page_tables;

	return process;
}

// NOTE: Associate and disassociate are a little bit silly, so it may be worthwhile to streamline
//       things by making it so that threads, upon creation, do this association, and on deletion,
//       do the disassociation
int process_associate_thread(struct ARC_Process *process, struct ARC_Thread *thread) {
	if (process == NULL || thread == NULL) {
		ARC_DEBUG(ERR, "Failed associate thread (%p) with process (%p)\n", thread, process);
		return -1;
	}

	struct ARC_Thread *temp = thread;
	ARC_ATOMIC_XCHG(&process->threads, &temp, &thread->next);

	return 0;
}

int process_disassociate_thread(struct ARC_Process *process, struct ARC_Thread *thread) {
	if (process == NULL || thread == NULL) {
		ARC_DEBUG(ERR, "Failed disassociate thread (%p) with process (%p)\n", thread, process);
		return -1;
	}

	struct ARC_Thread *current = process->threads;
	struct ARC_Thread *last = NULL;

	// NOTE: I do not see a way to avoid this while loop (it would be great if I could),
	//	 but the threads to do not keep track of the previous element, so I don't know
	while (current != NULL) {
		if (current == thread) {
			break;
		}

		last = current;
		current = current->next;
	}

	if (current == NULL) {
		ARC_DEBUG(ERR, "Could not find thread (%p) in process (%p)\n", thread, process);
		return -1;
	}

	struct ARC_Thread *temp = thread->next;
	if (last == NULL) {
		ARC_ATOMIC_XCHG(&process->threads, &temp, &thread->next);
	} else {
		ARC_ATOMIC_XCHG(&last->next, &temp, &thread->next);
	}

	return 0;
}

int process_fork(struct ARC_Process *process) {
	if (process == NULL) {
		ARC_DEBUG(ERR, "Failed to fork process, given process is NULL\n");
		return -1;
	}

	ARC_DEBUG(ERR, "Implement\n");

	return 0;
}

int process_delete(struct ARC_Process *process) {
	if (process == NULL) {
		ARC_DEBUG(ERR, "No process given\n");
		return -1;
	}

	return 0;
}

struct ARC_Thread *process_get_thread(struct ARC_Process *process) {
	if (process == NULL) {
		ARC_DEBUG(ERR, "Cannot get next thread of NULL process\n");
		return NULL;
	}
	
	struct ARC_Thread *ret = process->threads;
	uint32_t expected = ARC_THREAD_READY;

	while (ret != NULL) {
		if (ARC_ATOMIC_CMPXCHG(&ret->state, &expected, ARC_THREAD_RUNNING)) {
			break;
		}
		
		ret = ret->next;
	}

	return ret;
}
