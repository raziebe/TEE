#include "ImageManager.h"
#include "osal.h"
#include "common.h"
#include "AESencAPI.h"
#include "funcCipherStruct.h"
#include <linux/truly.h>

void im_init(PIMAGE_MANAGER manager,
	           void *driver_context,
	           PMemoryLayout memory_layout) {
	manager->gid = 0;
	manager->first_active_image = NULL;
	manager->driver_context = driver_context;
	manager->memory_layout = memory_layout;
}

static void im_free_encrypted_block(PENCRYPTED_BLOCK block)
{
	tp_free(block->dependencies);
	tp_free(block->encrypted_code);
	tp_free(block->original_code);
	tp_free(block->relocations);
}

void im_free_image(PACTIVE_IMAGE image)
{
	PENCRYPTED_BLOCK b, temp;
	for (b = image->first_encrypted_block; b;)
	{
		im_free_encrypted_block(b);
		temp = b;
		b = b->next;
		tp_free(temp);
	}
	tp_free(image->tags);
	tp_free(image->text_section);
	tp_free(image);
}

void im_clear(PIMAGE_MANAGER manager)
{
	PACTIVE_IMAGE image, temp;
	manager->gid = 0;
	TPmemset(manager->username, 0, sizeof(manager->username));
	TPmemset(manager->password, 0, sizeof(manager->password));
	for (image = manager->first_active_image; image;)
	{
		temp = image;
		image = image->next;
		im_free_image(temp);
	}
	manager->first_active_image = NULL;
}

BOOLEAN im_is_empty(PIMAGE_MANAGER manager)
{
	return !manager->first_active_image;
}

static UINT32 im_count_active_images(PIMAGE_MANAGER manager)
{
	UINT32 count = 0;
	PACTIVE_IMAGE image;
	for (image = manager->first_active_image; image; image = image->next)
		++count;
	return count;
}

PACTIVE_IMAGE im_add_image(PIMAGE_MANAGER manager, UINT64 pid, INT_PTR actualBase)
{
	PACTIVE_IMAGE image = tp_alloc(sizeof(ACTIVE_IMAGE));
	image->pid = pid;
	image->actualBase = actualBase;
	image->first_encrypted_block = NULL;
	image->next = manager->first_active_image;
	manager->first_active_image = image;
	return image;
}

BOOLEAN im_is_process_exists(PIMAGE_MANAGER manager, size_t pid)
{
	PACTIVE_IMAGE image;
	for (image = manager->first_active_image; image; image = image->next)
		if (image->pid == pid)
			return TRUE;
	return FALSE;

}
void im_remove_process(PIMAGE_MANAGER manager, size_t pid)
{
	if (!im_is_process_exists(manager,pid))
		return;

//	HypercallWithArg(HC_PROC_EXIT, pid);

	{
		PACTIVE_IMAGE *it, image;
		for (it = &(manager->first_active_image); *it ; )
		{
			if ((*it)->pid == pid)
			{
				image = *it;
				*it = image->next;
				im_free_image(image);
			}
			else
				it = &(*it)->next;
		}
	}
}

PACTIVE_IMAGE im_get_image(PIMAGE_MANAGER manager, size_t pid, size_t ip)
{
	PACTIVE_IMAGE image;

	for (image = manager->first_active_image; image; image = image->next)
	{
		if (image->pid == pid)
			if (im_get_block_by_ip(image, ip) != NULL)
				return image;
	}
	return NULL;
}



char* im_add_encrypted_block(PACTIVE_IMAGE process,
	                         char *tp_section,
							 PIMAGE_FILE image_file,
							 INT_PTR actual_base,
							 UINT64 image_base)
{
#define TOTAL_SIZE(BUF) (*(UINT32*)BUF + sizeof(UINT32))
	unsigned long code_base;
	struct truly_vm *tvm;
	PENCRYPTED_BLOCK block = tp_alloc(sizeof(ENCRYPTED_BLOCK));
	int relocationSize;

	block->start = *(PUINT32)tp_section;
	tp_section += 4;
	block->length = *(PUINT32)tp_section;
	tp_section += 4;

	block->original_code = tp_alloc(block->length);
	TPmemcpy(block->original_code, image_file_get_function_at_offset(image_file, block->start), block->length);

	tvm = get_tvm();
	code_base = image_file->code_base + tvm->enc->seg[0].pad_func_offset;
	tvm->enc->seg[0].pad_func_offset = block->start;
	tvm->enc->seg[0].size = block->length;

	tp_info("Pad Segment starts %lx offset %d size %d\n",
			code_base,
			tvm->enc->seg[0].pad_func_offset,
			tvm->enc->seg[0].size);

	block->start += actual_base;

	TPmemcpy(block->encrypted_code_tag, tp_section, MAC_TAG_SIZE);
	tp_section += MAC_TAG_SIZE;
	block->encrypted_code = tp_alloc(TOTAL_SIZE(tp_section));
	TPmemcpy(block->encrypted_code, tp_section, TOTAL_SIZE(tp_section));
	tp_section += TOTAL_SIZE(tp_section);

	TPmemcpy(block->dependencies_tag, tp_section, MAC_TAG_SIZE);
	tp_section += MAC_TAG_SIZE;
	block->dependencies = tp_alloc(TOTAL_SIZE(tp_section));
	TPmemcpy(block->dependencies, tp_section, TOTAL_SIZE(tp_section));
	tp_section += TOTAL_SIZE(tp_section);

	TPmemcpy(block->relocation_tag, tp_section, MAC_TAG_SIZE);
	tp_section += MAC_TAG_SIZE;
	relocationSize = TOTAL_SIZE(tp_section);
	block->relocations = tp_alloc(relocationSize);
	TPmemcpy(block->relocations, tp_section, relocationSize);
	tp_section += relocationSize;


	block->relocationTable.count = (relocationSize - 4) / sizeof(int);
	block->relocationTable.actual_base = actual_base;
	block->relocationTable.fix = (int)(actual_base - image_base);
	block->relocationTable.relocationArray = (int*)(block->relocations + 4);

	block->next = process->first_encrypted_block;
	process->first_encrypted_block = block;
	return tp_section;
#undef TOTAL_SIZE
}

PENCRYPTED_BLOCK im_get_block_by_ip(PACTIVE_IMAGE image, INT_PTR ip)
{
	PENCRYPTED_BLOCK block;
	for (block = image->first_encrypted_block; block; block = block->next)
	{
		INT_PTR diff = ip - block->start;
		if (diff >= 0 && diff < block->length)
			return block;
	}
	return NULL;
}

#define DEPENDENCY_MANAGER_MAX_IMAGES 256

BOOLEAN im_handle_image(PIMAGE_MANAGER im, PIMAGE_FILE image_file, size_t pid, INT_PTR actual_base)
{
	char *tp_section;
	char *tag;
	UINT32 gid, i, count,textSectionSize;
	UINT64 image_base;
	PACTIVE_IMAGE image;

	tp_section = image_file_get_tp_section(image_file);
	if (tp_section == NULL)
		return FALSE;

	tp_prepare_process(image_file);

	gid = *(PUINT32)(tp_section);
	tag = tp_section + 4;

	image_base = *(PUINT64)(tp_section + 20);
	count = *(PUINT32)(tp_section + 28);
	tp_section += 32;

	if (im->gid == 0)
		im->gid = gid;

	if (im_count_active_images(im) >= DEPENDENCY_MANAGER_MAX_IMAGES)
		return FALSE; // TODO: introduce some error reporting

	image = im_add_image(im, pid, actual_base);
	image->tags = tp_alloc(count * MAC_TAG_SIZE);
	image->tag_capacity = count;
	textSectionSize = image_file_get_text_section_size(image_file);
	TPmemcpy(image->text_section_tag, tag, MAC_TAG_SIZE);
	image->text_section = tp_alloc(textSectionSize + 4);
	*(UINT32*)image->text_section = textSectionSize;
	TPmemcpy(image->text_section + 4, image_file_get_text_section(image_file), textSectionSize);

	for (i = 0; i < count; ++i) {
		tp_section = im_add_encrypted_block(image, tp_section, image_file,
		                                    actual_base, image_base);

	}
	return TRUE;
}
/*
static BOOLEAN is_tag_valid(UCHAR *buffer, UCHAR *tag, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail)
{
	UCHAR result[MAC_TAG_SIZE];
//	UINT32 length = *(UINT32*)buffer;
	//  ??? mac_compute_tag(bufs, aes_ni_avail, buffer, length + sizeof(UINT32), key, result);
	return TPmemcmp(tag, result, MAC_TAG_SIZE) == 0;
}

BOOLEAN im_check_signature(PACTIVE_IMAGE image, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail)
{
	return is_tag_valid(image->text_section, image->text_section_tag, key, bufs, aes_ni_avail);
}

BOOLEAN im_is_code_valid(PENCRYPTED_BLOCK block, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail)
{
	return is_tag_valid(block->encrypted_code, block->encrypted_code_tag, key, bufs, aes_ni_avail);
}

BOOLEAN im_are_relocations_valid(PENCRYPTED_BLOCK block, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail)
{
	return is_tag_valid((UCHAR*)(block->relocations), block->relocation_tag, key, bufs, aes_ni_avail);
}
*/
UCHAR* im_get_encrypted_code(PENCRYPTED_BLOCK block)
{
	return block->encrypted_code + sizeof(UINT32);
}
/*
BOOLEAN im_are_dependencies_valid(PENCRYPTED_BLOCK block, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail)
{
	return is_tag_valid(block->dependencies, block->dependencies_tag, key, bufs, aes_ni_avail);
}
*/
UCHAR* im_get_dependency(PENCRYPTED_BLOCK block, UINT64 index)
{
	return block->dependencies + sizeof(UINT32) + (index + 1) * MAC_TAG_SIZE;
}

UINT32 im_get_dependencies_count(PENCRYPTED_BLOCK block)
{
	return (*(UINT32*)block->dependencies / MAC_TAG_SIZE) - 1;
}
