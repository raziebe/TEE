#pragma once

#include "tp_types.h"

#define IN_MEMORY

#define ELF_ARCH_64 2
#define ELF_ARCH_32 1

#define ELF_TYPE_EXEC 2
#define ELF_TYPE_DYN  3

BOOLEAN contains_dyn_symbol(size_t IN_MEMORY base,
						size_t* symbol_address,
						const char* sym_name);

BOOLEAN is_elf_file(struct file* file);

BOOLEAN get_elf_data (struct file* file, char* out_arch, char* out_type);

char* get_section_data( void* dest,
						unsigned long (*read)(void*,UINT64,char*,size_t),
						const char* name,
						size_t* base,
						size_t* offset,
						size_t* size);

BOOLEAN get_dynamic_table(size_t IN_MEMORY base,
					void** out, BOOLEAN* is_64_bit);
