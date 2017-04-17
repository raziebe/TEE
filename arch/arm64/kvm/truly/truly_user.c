#include <linux/module.h>
#include <linux/truly.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/sections.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <linux/truly.h>
#include "Aes.h"
#include "ImageFile.h"

DECLARE_PER_CPU(struct truly_vm, TVM);

unsigned long truly_get_ttbr0_el1(void)
{
	  long ttbr0_el1;

      asm("mrs %0,ttbr0_el1\n":"=r" (ttbr0_el1));
      return ttbr0_el1;
}

/*
 * Called from execve context.
 * Map the user
 */
void map_user_space_data(void *umem,int size,unsigned long vm_flags)
{
	int err;

 	err = create_hyp_user_mappings(umem, umem + size);
	if (err){
			tp_err(" failed to map ttbr0_el2\n");
			return;
	}

	tp_err("pid %d mapped %p flags=%ld\n", current->pid,umem ,vm_flags);
}

void unmap_user_space_data(unsigned long umem,int size)
{
	hyp_user_unmap(umem,  size);
	tp_err("pid %d unmapped %lx \n", current->pid, umem);
}

void tp_mark_protected(struct _IMAGE_FILE* image_file)
{
	int err;
	int cpu;
	struct truly_vm *tv;
	unsigned long ttbr0_el1 = truly_get_ttbr0_el1();

	for_each_possible_cpu(cpu) {
			tv = &per_cpu(TVM, cpu);
			tv->protected_pgd = ttbr0_el1;
	}

	tv = this_cpu_ptr(&TVM);
	tv->enc->seg[0].data = kmalloc(image_file->code_section_size, GFP_USER);

	memcpy(tv->enc->seg[0].data,image_file->tp_section,
			image_file->code_section_size);

	tv->enc->seg[0].size = (int)(*(tv->enc->seg[0].data + 0x24));
	tv->enc->seg[0].pad_data = NULL;

	err = create_hyp_mappings(tv->enc->seg[0].data,
 			tv->enc->seg[0].data + tv->enc->seg[0].size);

	if (err){
			tp_err(" failed to map tp_section\n");
			return;
	}

	tp_err("pid %d tp section "
			"mapped start %p  end = %p size %d \n",
			current->pid,
			tv->enc->seg[0].data,
			tv->enc->seg[0].data + tv->enc->seg[0].size,
			tv->enc->seg[0].size);
}
//
// for any process identified as
//
void tp_mmap_handler(unsigned long addr,int len,unsigned long vm_flags)
{
	map_user_space_data((void*)addr, len, vm_flags);
}

//
// for any process identified as
//
void tp_unmmap_handler(struct task_struct* task)
{
    struct vm_area_struct* p;

    if (!(task && task->mm && task->mm->mmap))
        return;

    for (p = task->mm->mmap; p ; p = p->vm_next) {
    	unsigned long base, size;

        base = p->vm_start;
        size = p->vm_end - base;
    	unmap_user_space_data(base, size);
    }
}

int __hyp_text truly_is_protected(struct truly_vm *tv)
{
	if (tv == NULL)
		tv = this_cpu_ptr(&TVM);
	return tv->protected_pgd == truly_get_ttbr0_el1();
}

void tp_unmark_protected(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
			struct truly_vm *tv = this_cpu_ptr(&TVM);
			tv->protected_pgd = 0;
	}
}

#include "AesC.h"

int tp_hyp_memcpy(char *dst,char *src,int size)
{
	int i;
	for (i = 0; i < size; i++)
		dst[i] = src[i];
	return i;
}

int tp_hyp_memset(char *dst,char tag,int size)
{
	int i;
	for (i = 0; i < size; i++)
		dst[i] = tag;
	return i;
}


int __hyp_text truly_decrypt(struct truly_vm *tv)
{
	int extra;
	int extra_offset;
	char extra_lines[16];
	int line = 0,lines = 0;
	unsigned char *d = NULL;
	unsigned char *pad = NULL;
	int data_offset = 60;
	UCHAR key[16+1] = {0};
	struct encrypt_tvm *enc;

	if (tv->protected_pgd != truly_get_ttbr0_el1()){
			return -1;
	}

	enc = (struct encrypt_tvm *) KERN_TO_HYP(tv->enc);

	if (enc->seg[0].pad_data == NULL) {
		enc->seg[0].pad_data = (char *)tv->elr_el2;
	}
	pad = enc->seg[0].pad_data;
	tv->brk_count_el2++;
	get_decrypted_key(key);
	lines = enc->seg[0].size/ 4;

	extra_offset = (enc->seg[0].size/ 16) * 16;
	extra = enc->seg[0].size - extra_offset;
	if ( extra > 0) {
		// backup extra code
		tp_hyp_memcpy(extra_lines, pad +  enc->seg[0].size, sizeof(extra_lines) - extra);
		// pad with zeros
		tp_hyp_memset(pad + enc->seg[0].size ,(char)0, sizeof(extra_lines) - extra );
		lines++;
	}

	d = (char *)KERN_TO_HYP(enc->seg[0].data);
	d += data_offset;

	for (line = 0 ; line < lines ; line += 4 ) {
		AESSW_Enc128( enc, d , pad, 1 ,key);
		d += 16;
		pad += 16;
	}

	if (extra > 0) {
		pad = enc->seg[0].pad_data;
		tp_hyp_memcpy( pad +  enc->seg[0].size, extra_lines ,sizeof(extra_lines) - extra);
	}
	return extra_offset;
}


int __hyp_text truly_pad(struct truly_vm *tv)
{
	char fault_cmd[4];
	int line = 0,lines = 0;
	unsigned char *pad;
	struct encrypt_tvm *enc;

	enc = (struct encrypt_tvm *) KERN_TO_HYP(tv->enc);

	pad = enc->seg[0].pad_data;
	lines = enc->seg[0].size / 4;

	if (tv->save_cmd != 0)
		tp_hyp_memcpy(fault_cmd, (char *)tv->save_cmd, sizeof(fault_cmd));

	for (line = 0 ; line < lines ; line++ ) {
		pad[4*line + 0] = 0x60;
		pad[4*line + 1] = 0x00;
		pad[4*line + 2] = 0x20;
		pad[4*line + 3] = 0xd4;
	}
	if (tv->save_cmd == 0)
		return 0xAAAAAAAA;
	/*
	* if the fault was in the padded function
	* we must put back the the command that generated the fault
	* ( for example svc ) and re commence it.
	*/
	if (tv->save_cmd < (unsigned long)pad){
			return 0xAAAAAAAB;
	}

	if (tv->save_cmd > ( (unsigned long)pad + enc->seg[0].size) ){
			return 0xAAAAAAAC;
	}
	//
	// Must put back the old command
	//
	tp_hyp_memcpy((char *)tv->save_cmd, fault_cmd, sizeof(fault_cmd));
	return lines;
}
