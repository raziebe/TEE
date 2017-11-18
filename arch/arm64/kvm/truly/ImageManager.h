#pragma once

#include "tp_types.h"
#include "ImageFile.h"
#include "memory_layout.h"
#include "Mac.h"

typedef struct
{
	int count;
	int fix;
	INT_PTR actual_base;
	int* relocationArray;
}RELOCATION_TABLE, *PRELOCATION_TABLE;

typedef struct _ENCRYPTED_BLOCK
{
	INT_PTR start;
	INT_PTR length;
	UCHAR *original_code;
	BOOLEAN original_code_copied;
	UCHAR encrypted_code_tag[MAC_TAG_SIZE];
	UCHAR *encrypted_code;
	UCHAR dependencies_tag[MAC_TAG_SIZE];
	UCHAR *dependencies;
	UCHAR relocation_tag[MAC_TAG_SIZE];
	UCHAR* relocations;
	RELOCATION_TABLE relocationTable;
	struct _ENCRYPTED_BLOCK* next;
} ENCRYPTED_BLOCK, *PENCRYPTED_BLOCK;

typedef struct _ACTIVE_IMAGE
{
	UINT64 pid;
	INT_PTR actualBase;
	PENCRYPTED_BLOCK first_encrypted_block;
	struct _ACTIVE_IMAGE* next;
	char *tags;
	UINT64 tag_capacity;
	UCHAR *text_section;
	UCHAR text_section_tag[MAC_TAG_SIZE];
} ACTIVE_IMAGE, *PACTIVE_IMAGE;

typedef struct
{
	UINT32 gid;
	char username[128];
	char password[64];
	PACTIVE_IMAGE first_active_image;
	void *driver_context;
	PMemoryLayout memory_layout;
} IMAGE_MANAGER, *PIMAGE_MANAGER;

void im_init(PIMAGE_MANAGER manager,
	           void *driver_context,
						 PMemoryLayout memory_layout);
void im_clear(PIMAGE_MANAGER manager);
BOOLEAN im_is_empty(PIMAGE_MANAGER manager);
void im_remove_process(PIMAGE_MANAGER manager, size_t pid);

PACTIVE_IMAGE im_get_image(PIMAGE_MANAGER manager, size_t pid, size_t ip);
PENCRYPTED_BLOCK im_get_block_by_ip(PACTIVE_IMAGE image, INT_PTR ip);
BOOLEAN im_handle_image(PIMAGE_MANAGER im, PIMAGE_FILE image_file, size_t pid, INT_PTR actual_base);
//BOOLEAN im_is_code_valid(PENCRYPTED_BLOCK block, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail);
UCHAR* im_get_encrypted_code(PENCRYPTED_BLOCK block);
//BOOLEAN im_are_dependencies_valid(PENCRYPTED_BLOCK block, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail);
UCHAR* im_get_dependency(PENCRYPTED_BLOCK block, UINT64 index);
UINT32 im_get_dependencies_count(PENCRYPTED_BLOCK block);
//BOOLEAN im_check_signature(PACTIVE_IMAGE image, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail);
//BOOLEAN im_are_relocations_valid(PENCRYPTED_BLOCK block, UCHAR *key, SW_AUX_BUFFERS *bufs, BOOLEAN aes_ni_avail);
BOOLEAN im_is_process_exists(PIMAGE_MANAGER manager, size_t pid);
