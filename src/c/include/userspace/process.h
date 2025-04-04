/**
 * @file process.h
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
#ifndef ARC_ARCH_PROCESS_H
#define ARC_ARCH_PROCESS_H

#include "lib/atomics.h"
#include <mm/vmm.h>
#include <config.h>
#include <stdint.h>
#include <stddef.h>
#include <userspace/thread.h>

struct ARC_Process {
	struct ARC_VMMMeta *allocator;
	struct ARC_Thread *threads;
	void *page_tables;
	void *base;
	struct ARC_File *file_table[ARC_PROCESS_FILE_LIMIT];
	int priority;
};

struct ARC_Process *process_create_from_file(int userspace, char *filepath);
struct ARC_Process *process_create(int userspace, void *page_tables);
int process_associate_thread(struct ARC_Process *process, struct ARC_Thread *thread);
int process_disassociate_thread(struct ARC_Process *process, struct ARC_Thread *thread);
int process_fork(struct ARC_Process *process);
int process_delete(struct ARC_Process *process);
struct ARC_Thread *process_get_thread(struct ARC_Process *process);

#endif
