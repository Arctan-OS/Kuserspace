/**
 * @file sysv.h
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
#include "userspace/thread.h"
#include <global.h>
#include <userspace/convention/sysv.h>
#include <lib/util.h>
#include <stdint.h>

#define AT_NULL 0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8 
#define AT_ENTRY 9
#define AT_LIBPATH 10
#define AT_FPHW 11
#define AT_INTP_DEVICE 12
#define AT_INTP_INODE 13

#define STACK_PUSH(__rsp, __val) __rsp -= 8; __rsp[0] = __val;

int sysv_prepare_entry_stack(ARC_Thread *thread, struct ARC_ELFMeta *meta, char **env, int envc, char **argv, int argc) {
        uint64_t *rsp = (uint64_t *)thread->pstack;

        if (rsp == NULL) {
                return -1;
        }

        // Push ENV
        uint64_t *rbp_env = rsp;
        
        for (int i = envc - 1; i >= 0; i--) {
                size_t off = strlen(env[i]);

                rsp -= off + 2;
                memcpy(rsp, &env[i], off);
                rsp[off] = 0;
        }

        // Push ARG
        uint64_t *rbp_arg = rsp;

        for (int i = argc - 1; i >= 0; i--) {
                size_t off = strlen(argv[i]);

                rsp -= off + 2;
                memcpy(rsp, &argv[i], off);
                rsp[off] = 0;
        }

        // Push vectors
        STACK_PUSH(rsp, 0);
        STACK_PUSH(rsp, 0);

        STACK_PUSH(rsp, (uint64_t)meta->entry);
        STACK_PUSH(rsp, AT_ENTRY);

        // Pad
        STACK_PUSH(rsp, 0);

        // Push ENV ptrs
        for (int i = envc - 1; i >= 0; i--) {
                size_t off = strlen(env[i]);

                rbp_env -= off + 2;
                STACK_PUSH(rsp, (uintptr_t)rbp_env);
        }

        // Pad
        STACK_PUSH(rsp, 0);

        // Push ARG ptrs
        for (int i = argc - 1; i >= 0; i--) {
                size_t off = strlen(argv[i]);

                rbp_arg -= off + 2;
                STACK_PUSH(rsp, (uintptr_t)rbp_arg);
        }

        STACK_PUSH(rsp, argc);

#ifdef ARC_TARGET_ARCH_X86_64
        thread->context.frame.rsp = (uintptr_t)rsp;
#endif

        return 0;
}
