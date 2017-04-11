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

#include "ImageFile.h"
#include "osal.h"
#include "linux_kernfile.h"
#include "elf.h"



static size_t read_from_file(void* file, UINT64 offset, char* dest, size_t amount)
{
	if (!file)
		return 0;

	return file_read((struct file*)file, offset, dest, amount);
}
/*
static UINT64 get_text_section_offset(PIMAGE_FILE image_file, UINT64 tp_section_offset)
{
	UINT64 text_sec_offset;
	if (!image_file)
		return 0;
	if (copy_from_user(&text_sec_offset, (char*)tp_section_offset+34, sizeof(UINT64)))
		return 0;
	return text_sec_offset;
}
*/
char* image_file_get_tp_section(PIMAGE_FILE image_file)
{
	return image_file->tp_section;
}

char* image_file_get_function_at_offset(PIMAGE_FILE image_file, UINT64 offset)
{
	return image_file->code_section + offset - (image_file->code_base - image_file->image_base);
}

static inline size_t get_task_image_base(struct task_struct* task)
{
    return task ? (task->mm? (task->mm->mmap ? task->mm->mmap->vm_start : 0) : 0) : 0;
}

void image_file_free(PIMAGE_FILE image_file)
{
	if (image_file != NULL)
	{
		if (image_file->tp_section != NULL)
			tp_free(image_file->tp_section);
		if (image_file->code_section != NULL)
			tp_free(image_file->code_section);
		tp_free(image_file);
	}
}

PIMAGE_FILE image_file_init(void* data)
{
	PIMAGE_FILE image_file;
	struct file* file;
	size_t offset, sz;

	if ((image_file = (PIMAGE_FILE)tp_alloc(sizeof(IMAGE_FILE))) == NULL)
		return NULL;

	file = (struct file*)data;

	image_file->tp_section = get_section_data(file,
											  read_from_file,
											  ".trulyP2",
											  NULL,
											  &offset,
											  NULL);

	image_file->tpsec_offset = offset;

	image_file->code_section = get_section_data(file,
												read_from_file,
												".text",
												NULL,
												&offset,
												&sz);
	image_file->code_offset = offset;
	image_file->code_section_size = sz;

	return image_file;
}

void image_file_init_bases(PIMAGE_FILE image_file, UINT64 actual_image_base)
{
	image_file->image_base = actual_image_base;
	image_file->code_base = image_file->image_base + image_file->code_offset;
}

char* image_file_get_text_section(PIMAGE_FILE image_file)
{
	return image_file->code_section;
}

UINT32 image_file_get_text_section_size(PIMAGE_FILE image_file)
{
	return image_file->code_section_size;
}

UINT64 image_file_get_assumed_image_base(PIMAGE_FILE image_file)
{
	return /*image_file->assumed_image_base;*/ image_file->image_base;
}

size_t image_file_get_tp_section_offset(PIMAGE_FILE image_file)
{
	return image_file->tpsec_offset;
}
