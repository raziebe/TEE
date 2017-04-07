#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/path.h>
#include <linux/mm_types.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/elf.h>

#include "elf.h"
#include "osal.h"
#include "linux_kernfile.h"

#define FIELD(p,type,field) (((type*)p)->field)

static BOOLEAN __section_data(void* dest, 
							  unsigned long (*read)(void*,UINT64,char*,size_t),
							  const char* name,
							  size_t* sec_offset,
							  size_t* sec_size,
							  size_t* sec_va);

BOOLEAN is_elf_file(struct file* file)
{
    unsigned int magic;
    return      file_read(file, 0, (char*)&magic, 4) == 4  &&
                magic == 0x464C457F;
}

BOOLEAN get_elf_data (struct file* file, char* out_arch, char* out_type)
{
    if (!(file != NULL && is_elf_file(file)))
        return FALSE;

    if (file_read(file, 0x4, out_arch, sizeof(*out_arch)) != sizeof(*out_arch))
		return FALSE;

    if (file_read(file, 0x10, out_type, sizeof(*out_type)) != sizeof(*out_type))
        return FALSE;

    return TRUE;
}

BOOLEAN get_dynamic_table(size_t IN_MEMORY base,
						  void** out, 
						  BOOLEAN* is_64)
{
	char arch;
	BOOLEAN is_64_bit;
	UINT32 sz;
	UINT32 hdrsz;
	char* p;
	size_t no_prog_hdrs, addr = 0;
	int i;
	BOOLEAN found = FALSE;

	if (copy_from_user(&arch, (char*)base+4, sizeof(arch)))
		return FALSE;

	is_64_bit = arch == 2;

	*is_64 = is_64_bit;

	hdrsz = is_64_bit ? sizeof(Elf64_Ehdr) : sizeof(Elf32_Ehdr);
	p = (char*)tp_alloc(hdrsz);
	if (!p)
		return FALSE;
	if (copy_from_user(p, (char*)base, hdrsz))
	{
		tp_free(p);
		return FALSE;
	}

	no_prog_hdrs = is_64_bit ? FIELD(p, Elf64_Ehdr, e_phnum)
											: FIELD(p, Elf32_Ehdr, e_phnum);
	tp_free(p);
	sz = is_64_bit ? sizeof(Elf64_Phdr) : sizeof(Elf32_Phdr);
	for (i = 0; i < no_prog_hdrs; ++i)
	{
		int type;
		size_t size;

		p = (char*)tp_alloc(sz);

		if (copy_from_user(p, (char*)base + hdrsz + i*sz, sz)){
			return -1;
		}
		type = is_64_bit ? FIELD(p, Elf64_Phdr, p_type)
				: FIELD(p, Elf32_Phdr, p_type);
		if (type == PT_DYNAMIC)
		{
			addr = is_64_bit ? FIELD(p, Elf64_Phdr, p_vaddr)
									: FIELD(p, Elf32_Phdr, p_vaddr);
			size = is_64_bit ? FIELD(p, Elf64_Phdr, p_memsz) 
									: FIELD(p, Elf32_Phdr, p_memsz);
			found = TRUE;
		}
		tp_free(p);
		if (found)
			break;
	}

	if (!found)
		return FALSE;

	{
		UINT8 entry_size = is_64_bit ? sizeof(UINT64) : sizeof(UINT32);

		i = 0;
		for (;;)
		{
			size_t type;
			p = tp_alloc(entry_size);
			if (copy_from_user(p, (char*)addr+i*entry_size, entry_size))
				return -1;
			type = is_64_bit ? *(UINT64*)p : *(UINT32*)p;
			++i;
			if (type == DT_NULL)
				break;
		}
		*out = tp_alloc(entry_size*i);
		if ( copy_from_user(*out, (char*)addr, i*entry_size))
			return -1;
	}
	return TRUE;
}

BOOLEAN contains_dyn_symbol(size_t IN_MEMORY base,
						size_t* symbol_address,
						const char* sym_name)
{
	char arch;
	void* base_ptr = (void*)base;
	BOOLEAN is_64_bit;
	UINT32 sz;
	UINT32 hdrsz;
	char* p;
	size_t no_prog_hdrs, offset = 0;
	int i;

	*symbol_address = 0;
	if (copy_from_user(&arch, (char*)base_ptr+4, sizeof(arch)))
		return FALSE;

	is_64_bit = arch == 2;

	hdrsz = is_64_bit ? sizeof(Elf64_Ehdr) : sizeof(Elf32_Ehdr);
	p = (char*)tp_alloc(hdrsz);
	if (!p)
		return FALSE;
	if (copy_from_user(p, base_ptr, hdrsz))
	{
		tp_free(p);
		return FALSE;
	}

	no_prog_hdrs = is_64_bit ? FIELD(p, Elf64_Ehdr, e_phnum)
											: FIELD(p, Elf32_Ehdr, e_phnum);

	tp_free(p);
	sz = is_64_bit ? sizeof(Elf64_Phdr) : sizeof(Elf32_Phdr);
	for (i = 0; i < no_prog_hdrs; ++i)
	{
		#define DYNAMIC_PHDR 2
		int type;
		p = (char*)tp_alloc(sz);

		if (copy_from_user(p, (char*)base_ptr + hdrsz + i*sz, sz))
			return -1;
		type = is_64_bit ? FIELD(p, Elf64_Phdr, p_type)
					: FIELD(p, Elf32_Phdr, p_type);
		if (type == DYNAMIC_PHDR)
		{
			offset = is_64_bit ? FIELD(p, Elf64_Phdr, p_vaddr)
								: FIELD(p, Elf32_Phdr, p_vaddr);
			sz = is_64_bit ? FIELD(p, Elf64_Phdr, p_memsz) 
								: FIELD(p, Elf32_Phdr, p_memsz);
		}
		tp_free(p);
		if (offset)
			break;
	}
	if (!offset)
		return FALSE;

	{
		UINT8 entry_size = is_64_bit ? sizeof(UINT64) : sizeof(UINT32);
		size_t dstroff = 0, dsymoff = 0;
		size_t dstrsz = 0;
		BOOLEAN terminated = FALSE;

		if (sz % entry_size)
			return FALSE;

		for (i = 0; !terminated; ++i)
		{
			#define DT_SYMTAB 6
			#define DT_STRTAB 5
			#define DT_NULL 0
			#define DT_STRSZ 10
			size_t type, value;
			p = tp_alloc(entry_size);

			if (copy_from_user(p, (char*)base_ptr+offset+i*entry_size, entry_size))
				return -1;

			type = is_64_bit ? *(UINT64*)p : *(UINT32*)p;
			if  ( copy_from_user(p, (char*)base_ptr+offset+(i+1)*entry_size, entry_size))
				return -1;
			value = is_64_bit ? *(UINT64*)p : *(UINT32*)p;
			tp_free(p);

			switch (type)
			{
				case DT_STRSZ:
					dstrsz = value;
					break;
				case DT_SYMTAB:
					dsymoff = value;
					break;
				case DT_STRTAB:
					dstroff = value;
					break;
				case DT_NULL:
					terminated = true;
			}
			if (dstrsz && dsymoff && dstroff)
				break;
		}

		if (!dstroff || !dsymoff || !dstrsz)
			return FALSE;

		sz = is_64_bit ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
		i = 0;
		for (;;)
		{
			size_t str_indx;
			size_t sym_val;
			p = tp_alloc(sz);
			if (copy_from_user(p, (char*)base + dsymoff+i*sz, sz))
			{
				tp_free(p);
				return FALSE;
			}
			str_indx = is_64_bit ? FIELD(p, Elf64_Sym, st_name)
									: FIELD(p, Elf32_Sym, st_name);

			if (0 == strcmp(sym_name, (char*)base + dstroff + str_indx))
			{
				KdPrint(("Found!\n"));
				sym_val = is_64_bit ? FIELD(p, Elf64_Sym, st_value)
									: FIELD(p, Elf32_Sym, st_value);

				KdPrint(("sym_val=%lx\n", sym_val));
				*symbol_address = base + sym_val;

				tp_free(p);
				return TRUE;
			}
			tp_free(p);
			if ((++i*sz + dsymoff) >= dstroff)
				break;
		}
		return FALSE;
	}
}

char* get_section_data( void* dest,
						unsigned long (*read)(void*,UINT64,char*,size_t),
						const char* name,
						size_t* base,
						size_t* offset,
						size_t* size)
{	

	size_t off=0, sz=0, va=0;
	char* section_data;

	if (!__section_data(dest, read, name, &off, &sz, &va))
		return NULL;

	section_data = (char*)tp_alloc(sz);
	if (!section_data) 
		return NULL;

	if (read(dest, off, section_data, sz) != sz)
	{
		tp_free(section_data);
		return NULL;
	}

	if (base)
		*base = va;
	if (offset)
		*offset = off;
	if (size)
		*size = sz;

	return section_data;
}	

static BOOLEAN __section_data(void* dest, 
							  unsigned long (*read)(void*,UINT64,char*,size_t),
							  const char* name,
							  size_t* sec_offset,
							  size_t* sec_size,
							  size_t* sec_va)
{
	char arch, *p, *strtbl;
	UINT16 sz;
	BOOLEAN is_64_bit, found = FALSE;
	size_t section_hdrs_offset, no_section_hdrs, str_tbl_indx;
	size_t strtbl_size, strtbl_offset;
	int i;
	struct file* file;

	if (!dest)
		return FALSE;

	file = (struct file*)dest;
	if (read(file, 4, &arch, sizeof(arch)) != sizeof(arch))
		return FALSE;

	is_64_bit = arch == 2;

	sz = is_64_bit ? sizeof(Elf64_Ehdr) : sizeof(Elf32_Ehdr);
	p = (char*)tp_alloc(sz);
	if (!p)
		return FALSE;
	if (read(file, 0, p, sz) != sz)
	{
		tp_free(p);
		return FALSE;
	}

	section_hdrs_offset = is_64_bit ? FIELD(p, Elf64_Ehdr, e_shoff)
											: FIELD(p, Elf32_Ehdr, e_shoff);

	no_section_hdrs = is_64_bit ? FIELD(p, Elf64_Ehdr, e_shnum)
										   : FIELD(p, Elf32_Ehdr, e_shnum);

	str_tbl_indx = is_64_bit ? FIELD(p, Elf64_Ehdr, e_shstrndx)
										: FIELD(p, Elf32_Ehdr, e_shstrndx);

	//KdPrint(("section_hdrs_offset=%lx\n",section_hdrs_offset));
	//KdPrint(("no_section_hdrs=%lx\n",no_section_hdrs));
	//KdPrint(("str_tbl_indx=%lx\n",str_tbl_indx));
	tp_free(p);

	// Grab string-table section header
	sz = is_64_bit ? sizeof(Elf64_Shdr) : sizeof(Elf32_Shdr);
	p = (char*)tp_alloc(sz);
	if (!p)
		return FALSE;
	
	if (read(file, section_hdrs_offset + sz*str_tbl_indx, p, sz) != sz)
	{
		tp_free(p);
		return FALSE;
	}

	strtbl_size = is_64_bit ? FIELD(p, Elf64_Shdr, sh_size)
								   : FIELD(p, Elf32_Shdr, sh_size);
	strtbl_offset = is_64_bit ? FIELD(p, Elf64_Shdr, sh_offset)
							  	   : FIELD(p, Elf32_Shdr, sh_offset);

	tp_free(p);

	strtbl = (char*)tp_alloc(strtbl_size);
	if (!strtbl)
		return FALSE;
	
	if (read(file, strtbl_offset, strtbl, strtbl_size) != strtbl_size)
		goto done;

	for (i = 0; i < no_section_hdrs; ++i)
	{
		#define MAX_SECTION_NAME_LEN 100
		char section_name[MAX_SECTION_NAME_LEN];
		size_t name_indx;
		sz = is_64_bit ? sizeof(Elf64_Shdr) : sizeof(Elf32_Shdr);
		p = (char*)tp_alloc(sz);
		if (!p)
			goto done;

		if (read(file, i*sz + section_hdrs_offset, p, sz) != sz)
		{
			tp_free(p);
			goto done;
		}

		name_indx = is_64_bit ? FIELD(p, Elf64_Shdr, sh_name)
									 : FIELD(p, Elf32_Shdr, sh_name);
		strcpy(section_name, strtbl + name_indx);

		if (0 == strcmp(section_name, name))
			found = true;
		tp_free(p);
		if (found)
		{
			*sec_offset =  is_64_bit ? FIELD(p, Elf64_Shdr, sh_offset) : FIELD(p, Elf32_Shdr, sh_offset);
			*sec_size = is_64_bit ? FIELD(p, Elf64_Shdr, sh_size) : FIELD(p, Elf32_Shdr, sh_size);
			*sec_va = is_64_bit ? FIELD(p, Elf64_Shdr, sh_addr) : FIELD(p, Elf32_Shdr, sh_addr);
			break;
		}
	}

done:
	tp_free(strtbl);
	return found;
}
