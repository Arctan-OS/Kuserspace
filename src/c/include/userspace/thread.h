/**
 * @file thread.h
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
#ifndef ARC_ARCH_THREAD_H
#define ARC_ARCH_THREAD_H

#include "arch/context.h"
#include "lib/spinlock.h"
#include "mp/profiling.h"

#include <stdint.h>
#include <stddef.h>

typedef struct ARC_Thread {
	struct ARC_Process *parent;
	void *pstack;
	void *vstack;
	size_t stack_size;
	uint64_t tid;
	ARC_Profile prof;
	ARC_Spinlock lock;
	uint32_t state;
	int priority; // If -1, use process's priority, otherwise, use this one
	ARC_Context *context;
} ARC_Thread;

ARC_Thread *thread_create(struct ARC_Process *process, void *entry, size_t stack_size);
int thread_delete(ARC_Thread *thread);

#endif
