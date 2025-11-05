/**
 * @file process.h
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
#ifndef ARC_ARCH_PROCESS_H
#define ARC_ARCH_PROCESS_H

#include "arctan.h"
#include "config.h"
#include "mm/vmm.h"
#include "userspace/thread.h"
#include "util.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ARC_ThreadElement {
	struct ARC_ThreadElement *next;
	ARC_Thread *t;
} ARC_ThreadElement;

typedef struct ARC_Process {
	ARC_VMMMeta *allocator;
	ARC_ThreadElement *threads;
	// NOTE: The page_tables pointer here points to an HHDM address
	struct {
		void *user;
		void *kernel;
	} page_tables;
	struct ARC_File *file_table[ARC_PROCESS_FILE_LIMIT];
	uint64_t pid;
	ARC_ProcessorFeatures features; // Features needed by the process
	int priority;
	bool userspace;
} ARC_Process;
STATIC_ASSERT(sizeof(ARC_Process) >= PAGE_SIZE, "Kernel heap may leak into userspace, increase ARC_PROCESS_FILE_LIMIT");

ARC_Process *process_create(bool userspace, void *page_tables);
ARC_Process *process_create_from_file(bool userspace, char *filepath);
int process_associate_thread(ARC_Process *process, ARC_Thread *thread);
int process_disassociate_thread(ARC_Process *process, ARC_Thread *thread);
int process_fork(ARC_Process *process);
int process_delete(ARC_Process *process);
int process_swap_out(ARC_Process *process);
int process_swap_in(ARC_Process *process);

#endif
