#ifndef  __EXEC_PROT_DB_H_LINUX__
#define __EXEC_PROT_DB_H_LINUX__

#include "tp_types.h"
#include "tp_string.h"

#define NOPS_INSNS 0

#define KERNEL_NO_SEGS 3
#define KERNEL_IMG_NOTABLES 2
#define KERNEL_OBJ_NOTABLES 5
#define EXE_NOTABLES 2

#define KERNEL_IMG_SDATA 0
#define KERNEL_IMG_SMPL  1

#define EXE_IMG_RELOCS 0
#define EXE_IMG_SDATA 1

#define KO_IMG_RELOCS 0
#define KO_IMG_PERCPU 1
#define KO_IMG_INITRELOCS 2
#define KO_IMG_SDATA 3
#define KO_IMG_SMPL 4

#define KO_WSTRING_TBL 5

#define RELOC_REL_TYPE_GLOB 0
#define RELOC_REL_TYPE_PC   1

#define LOCAL_RELOC 0

#pragma pack(push, 1)

enum ep_module_group
{
    EP_MOD_KERNIMG = 0,
    EP_MOD_INITKO64,
    EP_MOD_KO64,
    EP_MOD_SO64,
    EP_MOD_EXEC64,
    EP_MOD_SO32,
    EP_MOD_EXEC32
};

struct ep_module_data
{
    enum ep_module_group group;
    UINT32 module_id;
    size_t per_cpu_base;
};

struct ep_kernel_module_data
{
    struct ep_module_data data;
    size_t per_cpu_base;
};

struct ExecProtDb_
{
	UINT32 magic;
	UINT32 version;
	UINT32 timestamp;
};

struct ep_group_module
{
    UINT64 group_size;
};

struct ep_group_hdr
{
    UINT32 size;
    UINT32 count;
};

struct ep_page_records
{
	UINT32 first_in_page;		// index 
	UINT16 record_count;		// no' in page
};

struct ep_wstring_table
{
	UINT32 size;
};

struct ep_module_data_table
{
    UINT32 count;
    UINT8  entry_size;
};

struct ep_basic_image_header
{
	UINT32 total_size;
};

struct ep_non_image_kernel_header
{
    struct ep_basic_image_header basic_hdr;
	UINT32 page_count;
	UINT16 name_length;
};

struct ep_kernel_image_segment
{
	UINT32 pg_offset;	// from the image kernel base
	UINT32 pg_count;		// no Of pages in segment
	UINT32 size;
};

struct ep_page_base
{
    char hash[20];
};

struct ep_kernel_image_page
{
	struct ep_page_base base;
	struct ep_page_records staticdata;
    struct ep_page_records smplocks;
};

struct ep_kernel_object_page
{
	struct ep_page_base base;
	struct ep_page_records relcations;
	//struct ep_page_records alternatives;
	//struct ep_page_records parainstructions;
	struct ep_page_records percpu;
	struct ep_page_records initrelocs;
	struct ep_page_records staticdata;
	struct ep_page_records smplocks;
};

struct ep_exe_page
{
	struct ep_page_base base;
	struct ep_page_records relocations;
	//struct ep_page_records alternatives;
	struct ep_page_records staticdata;
};

struct ep_relocation 
{
    UINT64 	offset_start		: 13;			// if most significant bit is 1, underflow
	UINT64  offset_end		    : 13;			// value will be offsetStart + 4/8 , if most significant bit is 1, overflow
	UINT64  sign			    : 1;			// (signed/unsigned) relocation calculations 
	UINT64  rel_type			: 1;			// pc/gl
	UINT64  el_type			    : 2;			// ext/loc or ext+loc base address calculations. 
	UINT64  index			    : 20;			// index of associated name in the current database's static data table; if index = 0, need to add only current's k.o loaded base address
	UINT64  length			    : 14;			// length of name wstring
	UINT64  value;
};

struct ep_static_replacement 
{
	UINT64  offset_start		: 13;	//
	UINT64  offset_end			: 13;	// the original instruction's length equals offsetEnd - offsetStart
	UINT64  index				: 30;   // index '0' and replacement '0' implies all instruction from Start to End should be filled with a 'nop'.
	UINT64  _					: 8;	// 
};

struct ep_smp_lock 
{
	UINT16	offset	            : 12;				//only one (and known) byte
	UINT16	_		            : 4;
};

#pragma pack(pop)

typedef struct ExecProtDb_ *PExecProtDb;
/*
PExecProtDb epdb_init (const wchar_t* path);

struct ep_basic_image_header* epdb_get_module(PExecProtDb db, struct ep_module_data* data);

BOOLEAN epdb_get_moduleid_by_group (PExecProtDb db, 
                                    enum ep_module_group grp, 
                                    UINT32* out_module_id,
                                    String* name,
                                    BOOLEAN is_init);

struct ep_basic_image_header* epdb_get_img (struct ep_group_hdr* grp, UINT32 module_id);
void epdb_get_img_name (struct ep_basic_image_header* the_img, String* name);
struct ep_module_data_table* epdb_get_table_at_index (struct ep_module_data_table* tbl, unsigned char index);

void epdb_get_module_name(PExecProtDb db, struct ep_module_data* md, String* name);

struct ep_page_base* epdb_get_exec_page (struct ep_basic_image_header* hdr, int offset,
                                    struct ep_module_data_table** out_tbl);

struct ep_page_base* epdb_get_kernelobject_page (struct ep_basic_image_header* hdr, int offset,
                                            struct ep_module_data_table** out_tbl);

struct ep_page_base* epdb_get_kernelimage_page (struct ep_basic_image_header* hdr, int offset,
                                           struct ep_module_data_table** out_tbl);

struct ep_group_hdr* epdb_get_group_by_type (struct ep_group_module* group_module, enum ep_module_group type);
*/
#endif // _H__
