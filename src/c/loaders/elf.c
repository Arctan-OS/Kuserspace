/**
 * @file elf.c
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
#include <fs/vfs.h>
#include <userspace/loaders/elf.h>
#include <global.h>
#include <arch/pager.h>
#include <lib/util.h>
#include <mm/allocator.h>
#include <mm/pmm.h>
#include <stdatomic.h>
#include <stdint.h>

uintptr_t get_phys_page(void *page_tables, uintptr_t virtual, int type) {
	(void)type;
	
	uintptr_t phys_address = (uintptr_t)pmm_alloc_page();

	if (phys_address == 0) {
		// Fail
		ARC_DEBUG(ERR, "Failed to allocate new page, quiting load\n");
		return 0;
	}

	if (pager_map(page_tables, virtual, ARC_HHDM_TO_PHYS(phys_address), PAGE_SIZE, (1 << ARC_PAGER_US) | (1 << ARC_PAGER_RW)) != 0) {
		// Fail
		ARC_DEBUG(ERR, "Failed to map in new page\n");
		pmm_free((void *)phys_address);
		return 0;
	}

	return phys_address;
}

static struct ARC_ELFMeta *elf_load64(void *page_tables, struct ARC_File *file) {
	ARC_DEBUG(INFO, "Loading 64-bit ELF file (%p)\n", file);

	struct ARC_ELFMeta *meta = (struct ARC_ELFMeta *)alloc(sizeof(*meta));

	if (meta == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate ELF metadata\n");
		return NULL;
	}

	struct Elf64_Ehdr *header = (struct Elf64_Ehdr *)alloc(sizeof(*header));

	if (header == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate header\n");
		return NULL;
	}

	vfs_seek(file, 0, SEEK_SET);
	vfs_read(header, 1, sizeof(*header), file);

	if (header->e_ident[ELF_EI_CLASS] != ELF_CLASS_64) {
		free(header);
		return NULL;
	}

	meta->entry = (void *)header->e_entry;
	
	// TODO: Take care of this later for dynamic programs
	// meta->phdr = (void *)header->e_phoff;
	// meta->phent = header->e_phentsize;
	// meta->phnum = header->e_phnum;

	uint32_t header_count = header->e_phnum;
	struct Elf64_Phdr *program_headers = (struct Elf64_Phdr *)alloc(sizeof(*program_headers) * header_count);

	if (program_headers == NULL) {
		free(header);
		ARC_DEBUG(ERR, "Failed to allocate section header\n");
		return NULL;
	}

	vfs_seek(file, header->e_phoff, SEEK_SET);
	vfs_read(program_headers, 1, sizeof(*program_headers) * header_count, file);

	ARC_DEBUG(INFO, "Entry address: 0x%"PRIx64"\n", header->e_entry);
	ARC_DEBUG(INFO, "Mapping program headers (%d headers):\n", header_count);

	uintptr_t phys = 0;
	uintptr_t last_base = 0;
	for (uint32_t i = 0; i < header_count; i++) {
		ARC_DEBUG(INFO, "\tHeader %d 0x%"PRIx64":0x%"PRIx64" 0x%"PRIx64", 0x%"PRIx64":0x%"PRIx64" B\n", i, program_headers[i].p_paddr, program_headers[i].p_vaddr, program_headers[i].p_offset, program_headers[i].p_memsz, program_headers[i].p_filesz);

		switch (program_headers[i].p_type) {
			case PT_LOAD: {
				size_t size = program_headers[i].p_memsz;
				size_t psize = program_headers[i].p_filesz;
				size_t msize = program_headers[i].p_memsz;
				size_t off = 0;
		
				while (off < size) {
					size_t vaddr = program_headers[i].p_vaddr + off;
					size_t jank = vaddr & 0xFFF;
					size_t copy_size = min(PAGE_SIZE - jank, msize - off);
		
					if (jank == 0 || program_headers->p_vaddr - last_base >= PAGE_SIZE) {
						phys = get_phys_page(page_tables, program_headers[i].p_vaddr + off, 0);
					}
		
					if (off < psize) {
						// read from file
						vfs_seek(file, off + program_headers[i].p_offset, SEEK_SET);
						vfs_read((void *)phys + jank, 1, copy_size, file);
					} else {
						// memset page to 0
						memset((void *)phys + jank, 0, copy_size);
					}
					
					off += copy_size;
				}

				break;
			}

			case PT_DYNAMIC: {
				ARC_DEBUG(WARN, "\t\tDynamic section TODO\n");
				break;
			}

			default: {
				ARC_DEBUG(WARN, "\t\tUndetermined section, skipping\n");
				break;
			}

		}
	}
	
	free(header);
	free(program_headers);

	return meta;
}

struct ARC_ELFMeta *load_elf(void *page_tables, struct ARC_File *file) {
	if (file == NULL) {
		ARC_DEBUG(ERR, "No file given to load\n");
		return NULL;
	}

	return elf_load64(page_tables, file);
}
