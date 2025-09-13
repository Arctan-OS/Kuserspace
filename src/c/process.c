/**
 * @file process.c
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
#include "arch/pager.h"
#include "fs/vfs.h"
#include "global.h"
#include "lib/atomics.h"
#include "lib/util.h"
#include "mm/allocator.h"
#include "mm/vmm.h"
#include "userspace/convention/sysv.h"
#include "userspace/loaders/elf.h"
#include "userspace/thread.h"
#include "userspace/process.h"

#define DEFAULT_MEMSIZE 0x1000 * 4096
#define DEFAULT_STACKSIZE 0x2000

static uint64_t pid_counter = 0;

// Expected that process->base will be set by the caller
struct ARC_Process *process_create(bool userspace, void *page_tables) {
	struct ARC_Process *process = (struct ARC_Process *)alloc(sizeof(*process));

	if (process == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate process\n");
		return  NULL;
	}

	memset(process, 0, sizeof(*process));

	if (page_tables == NULL) {
		if (!userspace) {
			// Not a userspace process
			page_tables = (void *)ARC_PHYS_TO_HHDM(Arc_KernelPageTables);
		} else {
			page_tables = pager_create_page_tables();

			if (page_tables == NULL) {
				ARC_DEBUG(ERR, "Failed to allocate page tables\n");
				free(process);
				return NULL;
			}

			// TODO: Possibly isolate the kernel from the userspace more completely such that user
			//       only has the most basic functions mapped and not the whole kernel?
			pager_clone(page_tables, (uintptr_t)&__KERNEL_START__, (uintptr_t)&__KERNEL_START__,
				    ((uintptr_t)&__KERNEL_END__ - (uintptr_t)&__KERNEL_START__), 0);
		}
	}

	process->page_tables = page_tables;
	process->pid = ARC_ATOMIC_INC(pid_counter);

	return process;
}

struct ARC_Process *process_create_from_file(bool userspace, char *filepath) {
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

	void *base = (void *)0x10000000000; // TODO: Determine this from ELF meta

	ARC_VMMMeta *vmm = init_vmm(base, DEFAULT_MEMSIZE);
	if (vmm == NULL) {
		// TODO: Destroy page maps, destroy currently allocated meta data
		//       free all memory mapped by ELF loader
		ARC_DEBUG(ERR, "Failed to create process allocator\n");
		return NULL;
	}
	process->allocator = vmm;

	struct ARC_Thread *main = thread_create(process, meta->entry, DEFAULT_STACKSIZE);
	if (main == NULL) {
		process_delete(process);
		ARC_DEBUG(ERR, "Failed to create main thread\n");
		return NULL;
	}

	char *argv[] = {"hello", "world"};
	sysv_prepare_entry_stack(main, meta, NULL, 0, argv, 2);

	free(meta);

	ARC_DEBUG(INFO, "Created process from file %s\n", filepath);

	return process;
}

int process_associate_thread(struct ARC_Process *process, struct ARC_Thread *thread) {
	if (process == NULL || thread == NULL) {
		ARC_DEBUG(ERR, "Improper arguments\n");
		return -1;
	}

	ARC_ThreadElement *elem = alloc(sizeof(*elem));
	elem->t = thread;
	ARC_ATOMIC_XCHG(&process->threads, &elem, &elem->next);

	return 0;
}

int process_disassociate_thread(struct ARC_Process *process, struct ARC_Thread *thread) {
	if (process == NULL || thread == NULL) {
		ARC_DEBUG(ERR, "Improper arguments\n");
		return -1;
	}

	ARC_ThreadElement *current = process->threads;
	ARC_ThreadElement *prev = NULL;
	while (current != NULL && current->t != thread) {
		prev = current;
		current = current->next;
	}

	if (current == NULL) {
		ARC_DEBUG(ERR, "Could not find thread\n");
		return -2;
	}

	ARC_ThreadElement *temp = NULL;
	ARC_ATOMIC_XCHG(&prev->next, &current->next, &temp);

	free(current);

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
