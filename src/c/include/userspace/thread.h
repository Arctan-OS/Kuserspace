/**
 * @file thread.h
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
#ifndef ARC_ARCH_THREAD_H
#define ARC_ARCH_THREAD_H

#include <mm/vmm.h>
#include <stdint.h>
#include <stddef.h>
#include <lib/atomics.h>
#include <arch/context.h>

enum {
	ARC_THREAD_RUNNING = 0,
	ARC_THREAD_READY,
	ARC_THREAD_SUSPEND
};

enum {
	ARC_THREAD_PROFILE_MEM = 0,
	ARC_THREAD_PROFILE_IO,
	ARC_THREAD_PROFILE_COUNT
};

struct ARC_Thread {
	struct ARC_Thread *next;
	void *pstack;
	void *vstack;
	void *tcb;
	size_t stack_size;
	struct {
		uint64_t io_requests;
		uint64_t suspensions;
		// TODO: MORE DATA POINTS!
	} profiling_data;
	ARC_GenericSpinlock lock;
	uint32_t profile;
	uint32_t state;
	struct ARC_Context context;
};

struct ARC_Thread *thread_create(struct ARC_VMMMeta *allocator, void *page_tables, void *entry, size_t stack_size);
int thread_delete(struct ARC_Thread *thread);
int thread_set_profile(struct ARC_Thread *thread, uint32_t profile);
uint32_t thread_get_profile(struct ARC_Thread *thread);

#endif
