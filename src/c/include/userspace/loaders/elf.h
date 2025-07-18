/**
 * @file elf.h
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
#ifndef ARC_USERSPACE_ELF_H
#define ARC_USERSPACE_ELF_H

#include <stdint.h>
#include <fs/vfs.h>

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

struct ARC_ELFMeta {
	void *entry;
	void *phdr;
	size_t phent;
	size_t phnum;
};

struct ARC_ELFMeta *load_elf(void *page_tables, struct ARC_File *data);

#endif
