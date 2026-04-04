/**
 * @file 64.c
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
#include "userspace/loader.h"

int load(ARC_ProgramMeta *, void *virt, size_t) {
        return -1;
}

int unload(ARC_ProgramMeta *, void *virt, size_t) {
        return -1;
}

int uninit(ARC_ProgramMeta *) {
        return -1;
}

int init(ARC_ProgramMeta *, ARC_File *) {
        return -1;
}

ARC_REGISTER_LOADER(ARC_LDRGRP_64BIT, elf) = {
        .init = init,
        .uninit = uninit,
        .load = load,
        .unload = unload,
};
