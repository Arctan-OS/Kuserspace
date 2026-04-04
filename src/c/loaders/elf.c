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

#include "userspace/loader.h"


#include "abi-bits/seek-whence.h"
#include <fs/vfs.h>
#include <userspace/loaders/elf.h>
#include <global.h>
#include <arch/pager.h>
#include <lib/util.h>
#include <mm/allocator.h>
#include <mm/pmm.h>
#include <stdatomic.h>
#include <stdint.h>

#define ELF_SHT_NULL     0
#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB   2
#define ELF_SHT_STRTAB   3
#define ELF_SHT_RELA     4
#define ELF_SHT_HASH     5
#define ELF_SHT_DYNAMIC  6
#define ELF_SHT_NOTE     7
#define ELF_SHT_NOBITS   8
#define ELF_SHT_REL      9
#define ELF_SHT_SHLIB    10
#define ELF_SHT_DYNSYM   11

#define ELF_EI_MAG0        0
#define ELF_EI_MAG1        1
#define ELF_EI_MAG2        2
#define ELF_EI_MAG3        3

#define ELF_EI_CLASS       4
#define ELF_CLASS_32       1
#define ELF_CLASS_64       2

#define ELF_EI_DATA        5
#define ELF_EI_VERSION     6

#define ELF_EI_OSABI       7
#define ELF_ABI_SYSV       0
#define ELF_ABI_HPUX       1
#define ELF_ABI_STANDALONE 2

#define ELF_EI_ABIVERSION  8
#define ELF_EI_PAD         9
#define ELF_EI_NIDENT      16

#define PT_NULL 	0 
#define PT_LOAD 	1 
#define PT_DYNAMIC 	2
#define PT_INTERP 	3
#define PT_NOTE 	4 
#define PT_SHLIB 	5
#define PT_PHDR 	6 
#define PT_LOOS 	0x60000000 
#define PT_HIOS 	0x6FFFFFFF
#define PT_LOPROC 	0x70000000 
#define PT_HIPROC 	0x7FFFFFFF
#define PF_X 		0x1 
#define PF_W 		0x2 
#define PF_R 		0x4 
#define PF_MASKOS 	0x00FF0000
#define PF_MASKPROC 	0xFF000000

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef uint64_t Elf64_Sxword;

struct Elf64_Ehdr {
	unsigned char e_ident[16]; /* ELF identification */
	Elf64_Half e_type; /* Object file type */
	Elf64_Half e_machine; /* Machine type */
	Elf64_Word e_version; /* Object file version */
	Elf64_Addr e_entry; /* Entry point address */
	Elf64_Off e_phoff; /* Program header offset */
	Elf64_Off e_shoff; /* Section header offset */
	Elf64_Word e_flags; /* Processor-specific flags */
	Elf64_Half e_ehsize; /* ELF header size */
	Elf64_Half e_phentsize; /* Size of program header entry */
	Elf64_Half e_phnum; /* Number of program header entries */
	Elf64_Half e_shentsize; /* Size of section header entry */
	Elf64_Half e_shnum; /* Number of section header entries */
	Elf64_Half e_shstrndx; /* Section name string table index */
}__attribute__((packed));

struct Elf64_Shdr {
	Elf64_Word sh_name; /* Section name */
	Elf64_Word sh_type; /* Section type */
	Elf64_Xword sh_flags; /* Section attributes */
	Elf64_Addr sh_addr; /* Virtual address in memory */
	Elf64_Off sh_offset; /* Offset in file */
	Elf64_Xword sh_size; /* Size of section */
	Elf64_Word sh_link; /* Link to other section */
	Elf64_Word sh_info; /* Miscellaneous information */
	Elf64_Xword sh_addralign; /* Address alignment boundary */
	Elf64_Xword sh_entsize; /* Size of entries, if section has table */
}__attribute__((packed));

struct Elf64_Sym {
	Elf64_Word st_name; /* Symbol name */
	unsigned char st_info; /* Type and Binding attributes */
	unsigned char st_other; /* Reserved */
	Elf64_Half st_shndx; /* Section table index */
	Elf64_Addr st_value; /* Symbol value */
	Elf64_Xword st_size; /* Size of object (e.g., common) */
}__attribute__((packed));

struct Elf64_Rel {
	Elf64_Addr r_offset; /* Address of reference */
	Elf64_Xword r_info; /* Symbol index and type of relocation */
}__attribute__((packed));

struct Elf64_Rela {
	Elf64_Addr r_offset; /* Address of reference */
	Elf64_Xword r_info; /* Symbol index and type of relocation */
	Elf64_Sxword r_addend; /* Constant part of expression */
}__attribute__((packed));

struct Elf64_Phdr {
	Elf64_Word p_type; /* Type of segment */
	Elf64_Word p_flags; /* Segment attributes */
	Elf64_Off p_offset; /* Offset in file */
	Elf64_Addr p_vaddr; /* Virtual address in memory */
	Elf64_Addr p_paddr; /* Reserved */
	Elf64_Xword p_filesz; /* Size of segment in file */
	Elf64_Xword p_memsz; /* Size of segment in memory */
	Elf64_Xword p_align; /* Alignment of segment */
}__attribute__((packed));

uintptr_t get_phys_page(void *page_tables, uintptr_t virtual, int type) {
	(void)type;
	
	uintptr_t phys_address = (uintptr_t)pmm_fast_page_alloc();

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

ARC_REGISTER_LOADER(ARC_LDRGRP_SOLO, elf64) = {
        .init = NULL,
        .uninit = NULL,
        .load = NULL,
        .unload = NULL,
};
