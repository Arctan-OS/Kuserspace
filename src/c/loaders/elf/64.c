/**
 * @file 64.c
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
#include "userspace/loaders/elf.h"
#include "userspace/loader.h"
#include "fs/vfs.h"
#include "mm/allocator.h"
#include "abi-bits/seek-whence.h"

static int load_page(ARC_ProgramMeta *meta, void *virt) {
        // TODO: Pager must be reworked for this
        // void *a = NULL
        // if ((a = pager_to_phys(page_tables, virt)) == NULL) {
        //         a = pmm_alloc(PAGE_SIZE);
        // }
        // vfs_seek(file, offset to data in file, SEEK_SET);
        // vfs_read(a + offset in page, 1, PAGE_SIZE - offset, file);
        
        return -1;
}

int load(ARC_ProgramMeta *meta, void *virt, size_t size) {
        struct ARC_ELFMeta *elf_meta = meta->loader_data;
        
        if (virt == NULL) {
                ARC_DEBUG(INFO, "Loading full program from memory\n");

                for (uint32_t i = 0; i < elf_meta->phdrs.count; i++) {
                        struct Elf64_Phdr header = elf_meta->phdrs.headers[i];
                        
                        ARC_DEBUG(INFO, "\tHeader %d 0x%"PRIx64":0x%"PRIx64" 0x%"PRIx64", 0x%"PRIx64":0x%"PRIx64" B\n", i, header.p_paddr, header.p_vaddr,
                                  header.p_offset, header.p_memsz, header.p_filesz);
                        
                        switch (header.p_type) {
                        case PT_LOAD: {
                                break;
                        }
                                
                        }
                }
                
                return -1;
        }

        for (uint32_t i = 0; i < elf_meta->phdrs.count; i++) {
                struct Elf64_Phdr *header = &elf_meta->phdrs.headers[i];

                if (header->p_vaddr <= (uintptr_t)virt && (uintptr_t)virt <= header->p_vaddr + header->p_memsz) {
                        return load_page(header, virt);
                }
        }
        
        return -1;
}

int unload(ARC_ProgramMeta *meta, void *virt, size_t size) {
        if (virt == NULL) {
                ARC_DEBUG(INFO, "Unloading full program from memory\n");
                return 0;
        }

        return -1;
}

int uninit(ARC_ProgramMeta *meta) {
        return -1;
}

int init(ARC_ProgramMeta *meta, ARC_File *file) {
        ARC_DEBUG(INFO, "Loading 64-bit ELF file (%p)\n", file);

	struct ARC_ELFMeta *elf_meta = (struct ARC_ELFMeta *)alloc(sizeof(*meta));
	if (meta == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate ELF metadata\n");
		return -1;
	}

        meta->loader_data = (void *)elf_meta;
        
	struct Elf64_Ehdr *header = (struct Elf64_Ehdr *)alloc(sizeof(*header));
	if (header == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate header\n");
                free(elf_meta);
		return -2;
	}

	vfs_seek(file, 0, SEEK_SET);
	vfs_read(header, 1, sizeof(*header), file);

        elf_meta->header = header;
        
	if (header->e_ident[ELF_EI_CLASS] != ELF_CLASS_64) {
		free(header);
                free(elf_meta);
		return -3;
	}

	meta->entry = (void *)header->e_entry;

	uint32_t header_count = header->e_phnum;
	struct Elf64_Phdr *program_headers = (struct Elf64_Phdr *)alloc(sizeof(*program_headers) * header_count);

	if (program_headers == NULL) {
		free(header);
                free(elf_meta);
		ARC_DEBUG(ERR, "Failed to allocate section header\n");
		return -4;
	}

	vfs_seek(file, header->e_phoff, SEEK_SET);
	vfs_read(program_headers, 1, sizeof(*program_headers) * header_count, file);

        elf_meta->phdrs.headers = program_headers;
        elf_meta->phdrs.count = header_count;
        elf_meta->phdrs.size = header->e_phentsize;
        
        ARC_DEBUG(INFO, "Entry address: 0x%"PRIx64"\n", header->e_entry);

        return 0;
}

ARC_REGISTER_LOADER(ARC_LDRGRP_64BIT, elf) = {
        .init = init,
        .uninit = uninit,
        .load = load,
        .unload = unload,
};
