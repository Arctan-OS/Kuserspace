/**
 * @file loader.h
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kuserspace - Kernel-Userspace Junction
 * Copyright (C) 2023-2026 awewsomegamer
 *
 * This file is part of Arctan-OS/Kuserspace
 *
 * Arctan-OS/Kuserspace is free software; you can redistribute it and/or
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
#ifndef ARC_USERSPACE_LOADER_H
#define ARC_USERSPACE_LOADER_H

#include <stddef.h>
#include "drivers/resource.h"

#define ARC_REGISTER_LOADER(group, name)         \
	ARC_ProgramLoaderDef _ldrdefs_##name##_##group

#define ARC_SHARE_LOADER_INDICES(...) ;

enum ARC_LOADER_GROUP {
        ARC_LDRGRP_64BIT = 0,
        ARC_LDRGRP_32BIT,
};

ARC_SHARE_LOADER_INDICES(ARC_LDRGRP_64BIT, ARC_LDRGRP_32BIT)

typedef struct ARC_ProgramMeta {
        void *entry;
        size_t size;
        void *loader_data;
        const struct ARC_ProgramLoaderDef *loader;
        void *page_table;
} ARC_ProgramMeta;
        
typedef struct ARC_ProgramLoaderDef {
        int (*load)  (ARC_ProgramMeta *, void *virt, size_t);
        int (*unload)(ARC_ProgramMeta *, void *virt, size_t);
        int (*uninit)(ARC_ProgramMeta *);
        int (*init)  (ARC_ProgramMeta *, ARC_File *);
} ARC_ProgramLoaderDef;

int program_loader_load(ARC_ProgramMeta *, void *, size_t);
int program_loader_unload(ARC_ProgramMeta *, void *, size_t);
int uninit_program_loader(ARC_ProgramMeta *);
ARC_ProgramMeta *init_program_loader(int group, int index, ARC_File *, void *page_table);

#endif
