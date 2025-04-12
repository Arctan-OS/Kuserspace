/**
 * @file syscall.c
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
#include <fs/vfs.h>
#include <config.h>
#include "arch/pager.h"
#include "arctan.h"
#include "lib/resource.h"
#include "mm/vmm.h"
#include "userspace/process.h"
#include <global.h>
#include <arch/smp.h>
#include <mp/scheduler.h>
#include <mm/pmm.h>

#include <arch/x86-64/ctrl_regs.h>

static int syscall_tcb_set(void *arg) {
	(void)arg;

	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	desc->current_thread->tcb = arg;
	_x86_WRMSR(0xC0000100, (uintptr_t)arg);

	// TCB_SET
	return 0;
}

static int syscall_futex_wait(int *ptr, int expected, struct timespec const *time) {
	(void)ptr;
	(void)expected;
	(void)time;

	printf("Futex wait\n");
	// FUTEX_WAIT
	return 0;
}
static int syscall_futex_wake(int *ptr) {
	(void)ptr;

	printf("Futex wake\n");
	// FUTEX_WAKE
	return 0;
}

static int syscall_clock_get(int a, long *b, long *c) {
	(void)a;
	(void)b;
	(void)c;

	printf("Syscall clock get\n");
	// CLOCK_GET
	return 0;
}

static int syscall_exit(int code) {
	(void)code;

	ARC_DEBUG(INFO, "Exiting %d\n", code);
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	sched_dequeue(desc->current_process);
	// process_delete(desc->current_process->process);

	return 0;
}

static int syscall_seek(int fd, long offset, int whence, long *new_offset) {
	(void)fd;
	(void)offset;
	(void)whence;
	(void)new_offset;

	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->current_process->process->file_table[fd];

	if (file == NULL) {
		return -1;
	}

	*new_offset = vfs_seek(file, offset, whence);

	return 0;
}

static int syscall_write(int fd, void const *buffer, unsigned long count, long *written) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->current_process->process->file_table[fd];

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
	struct ARC_File *file = desc->current_process->process->file_table[fd];

	if (file == NULL) {
		return -1;
	}

	*read = vfs_read((void *)buffer, 1, count, file);
	
	return 0;
}

static int syscall_close(int fd) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_File *file = desc->current_process->process->file_table[fd];

	if (file == NULL) {
		return -1;
	}

	if (vfs_close(file) == 0) {
		desc->current_process->process->file_table[fd] = NULL;
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
		if (desc->current_process->process->file_table[i] == NULL) {
			desc->current_process->process->file_table[i] = file;
			*fd = i;
			break;
		}
	}

	return 0;
}
static int syscall_vm_map(void *hint, unsigned long size, int prot, int flags, int fd, long offset, void **window) {
	(void)hint;
	(void)size;
	(void)prot;
	(void)flags;
	(void)fd;
	(void)offset;
	(void)window;

	printf("Mapping\n");
	// VM_MAP
	return 0;
}

static int syscall_vm_unmap(void *a, unsigned long b) {
	(void)a;
	(void)b;

	printf("Unmapping\n");
	// VM_UNMAP
	return 0;
}

static int syscall_anon_alloc(unsigned long size, void **ptr) {
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_VMMMeta *vmeta = desc->current_process->process->allocator;

	void *vaddr = vmm_alloc(vmeta, size);
	void *paddr = pmm_alloc(size);
	
	*ptr = vaddr;

	if (pager_map(NULL, (uintptr_t)vaddr, ARC_HHDM_TO_PHYS(paddr), size, (1 << ARC_PAGER_US) | (1 << ARC_PAGER_RW)) != 0) {
		*ptr = NULL;
		return -1;
	}
	
	return 0;
}

static int syscall_anon_free(void *ptr, unsigned long size) {
	(void)size;
	struct ARC_ProcessorDescriptor *desc = smp_get_proc_desc();
	struct ARC_VMMMeta *vmeta = desc->current_process->process->allocator;

	size_t vsize = vmm_free(vmeta, ptr);
	void *paddr = (void *)ARC_PHYS_TO_HHDM(pager_unmap(NULL, (uintptr_t)ptr, vsize));
	pmm_free(paddr);

	// ANON_FREE
	return 0;
}

static int syscall_libc_log(const char *str) {
	(void)str;
	
	// LIBC LOG
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
	[12] = syscall_anon_alloc,
	[13] = syscall_anon_free,
	[14] = syscall_libc_log,
};