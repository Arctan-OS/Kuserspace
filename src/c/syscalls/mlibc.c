/**
 * @file syscall.c
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
#include "abi-bits/seek-whence.h"
#include "arch/context.h"
#include <interface/terminal.h>
#include <fs/vfs.h>
#include <config.h>
#include <arch/pager.h>
#include <arch/x86-64/util.h>
#include <arctan.h>
#include <drivers/resource.h>
#include <mm/vmm.h>
#include <userspace/process.h>
#include <global.h>
#include <arch/smp.h>
#include <mp/scheduler.h>
#include <mm/pmm.h>

static int syscall_tcb_set(void *arg) {
	ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	context_set_tcb(desc->thread->context, arg);

	return 0;
}

static int syscall_futex_wait(int *ptr, int expected, struct timespec const *time) {
	(void)ptr;
	(void)expected;
	(void)time;

	printf("Futex wait\n");

	return 0;
}

static int syscall_futex_wake(int *ptr) {
	(void)ptr;

	printf("Futex wake\n");
	
	return 0;
}

static int syscall_clock_get(int a, long *b, long *c) {
	(void)a;
	(void)b;
	(void)c;

	printf("Syscall clock get\n");
	
	return 0;
}

static int syscall_exit(int code) {
	ARC_DEBUG(INFO, "Exiting %d\n", code);
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	term_draw();
	process_delete(desc->thread->parent);

	return 0;
}

static int syscall_seek(int fd, long offset, int whence, long *new_offset) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->thread->parent->file_table[fd];

	if (file == NULL) {
		return -1;
	}

	*new_offset = vfs_seek(file, offset, whence);

	return 0;
}

static int syscall_write(int fd, void const *buffer, unsigned long count, long *written) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->thread->parent->file_table[fd];

#ifdef ARC_DEBUG_ENABLE
	if (fd == 0) {
		printf("%.*s", count, buffer);
	}
#endif

	if (file == NULL) {
		return -1;
	}

	*written = vfs_write((void *)buffer, 1, count, file);
	
	return 0;
}

static int syscall_read(int fd, void *buffer, unsigned long count, long *read) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->thread->parent->file_table[fd];

	if (file == NULL) {
		return -1;
	}

	*read = vfs_read((void *)buffer, 1, count, file);
	
	return 0;
}

static int syscall_close(int fd) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->thread->parent->file_table[fd];

	if (file == NULL) {
		return -1;
	}

	if (vfs_close(file) == 0) {
		desc->thread->parent->file_table[fd] = NULL;
	} else {
		return -1;
	}

	return 0;
}

static int syscall_open(char const *name, int flags, unsigned int mode, int *fd) {
	struct ARC_File *file = NULL;
	if (vfs_open((char *)name, flags, mode, &file) != 0) {
		*fd = -1;
		return -1;
	}

	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	for (int i = 0; i < ARC_PROCESS_FILE_LIMIT; i++) {
		if (desc->thread->parent->file_table[i] == NULL) {
			desc->thread->parent->file_table[i] = file;
			*fd = i;
			break;
		}
	}

	return 0;
}

static int syscall_vm_map(void *hint, unsigned long size, uint64_t prot_flags, int fd, long offset, void **ptr) {
	int _prot = prot_flags >> 32;
	int _flags = prot_flags & UINT32_MAX;

	(void)_prot;
	(void)_flags;

	if (size == 0 || ptr == NULL) {
		return -1;
	}

	ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	ARC_VMMMeta *vmeta = desc->process->allocator;
	
	*ptr = NULL;
	
	void *paddr = pmm_alloc(size);

	if (paddr == NULL) {
		return -2;
	}

	retry:;

	void *vaddr = (hint == NULL ? vmm_alloc(vmeta, size) : hint);

	if (vaddr == NULL) {
		return -3;
	}

	// TODO: Properly set the flags
	uint32_t flags = (1 << ARC_PAGER_US) | (1 << ARC_PAGER_RW);

	if (pager_map(desc->process->page_tables.user, (uintptr_t)vaddr, ARC_HHDM_TO_PHYS(paddr), size, flags) != 0) {
		if (hint != NULL) {
			hint = NULL;
			goto retry;
		} else {
			return -4;
		}
	}

	// NOTE: This call can fail, the page fault handler should
	//       take care of the case when such a memory region needs
	//       to be accessed and isn't already mapped in
	pager_map(desc->process->page_tables.kernel, (uintptr_t)vaddr, ARC_HHDM_TO_PHYS(paddr), size, flags);

	if (fd >= 0 && fd <= ARC_PROCESS_FILE_LIMIT - 1) {
		struct ARC_File *file = desc->process->file_table[fd];

		if (file != NULL) {
			vfs_seek(file, offset, SEEK_SET);
			vfs_read(vaddr, 1, size, file);
			// TODO: Should the previous offset be restored?
		}
	}

	*ptr = vaddr;

	return 0;
}

static int syscall_vm_unmap(void *address, unsigned long size) {
	if (size == 0) {
		return -1;
	}

	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_VMMMeta *vmeta = desc->thread->parent->allocator;

	vmm_free(vmeta, address);
	void *paddr = NULL;
	if (pager_unmap(desc->process->page_tables.user, (uintptr_t)address, size, &paddr) != 0) {
		ARC_DEBUG(ERR, "I do not know how to recover from this\n");
		ARC_HANG;
	}

	if (pager_unmap(desc->process->page_tables.kernel, (uintptr_t)address, size, NULL) != 0) {
		ARC_DEBUG(ERR, "I do not know how to recover from this\n");
		ARC_HANG;
	}

	pmm_free(paddr);

	return 0;
}

static int syscall_libc_log(const char *str) {	
	printf("%s\n", str);

	return 0;
}

int (*Arc_SyscallTable[])() = {
	[0] = syscall_tcb_set,
	[1] = syscall_futex_wait,
	[2] = syscall_futex_wake,
	[3] = syscall_clock_get,
	[4] = syscall_exit,
	[5] = syscall_seek,
	[6] = syscall_write,
	[7] = syscall_read,
	[8] = syscall_close,
	[9] = syscall_open,
	[10] = syscall_vm_map,
	[11] = syscall_vm_unmap,
	[12] = syscall_libc_log,
};
