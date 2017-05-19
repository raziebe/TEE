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
#include <linux/delay.h>
#include "Aes.h"
#include "ImageFile.h"

DECLARE_PER_CPU(struct truly_vm, TVM);

unsigned long truly_get_ttbr0_el1(void)
{
	  long ttbr0_el1;

      asm("mrs %0,ttbr0_el1\n":"=r" (ttbr0_el1));
      return ttbr0_el1;
}

unsigned long  truly_get_exception_level(void)
{
	long el;
	asm ("mrs	%0, CurrentEl\n":"=r"(el));
	return el;
}


/*
 * Called from execve context.
 * Map the user
 */
void map_user_space_data(void *umem,int size,unsigned long vm_flags)
{
	int err;
	struct truly_vm *tv;
	struct hyp_addr* addr;

 	err = create_hyp_user_mappings(umem, umem + size);
	if (err){
			tp_err(" failed to map ttbr0_el2\n");
			return;
	}

	addr = kmalloc(sizeof(struct hyp_addr ),GFP_USER);
	tp_err("pid %d user mapped %p size=%d\n", current->pid,umem ,size);
	addr->addr = (unsigned long)umem;
	addr->size = size;
	tv = this_cpu_ptr(&TVM);
	list_add(&addr->lst, &tv->hyp_addr_lst);
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
	struct hyp_addr *addr;
	unsigned long ttbr0_el1 = truly_get_ttbr0_el1();

	for_each_possible_cpu(cpu) {
			tv = &per_cpu(TVM, cpu);
			tv->protected_pgd = ttbr0_el1;
	}

	tv = this_cpu_ptr(&TVM);
	tv->enc->seg[0].enc_data  = kmalloc(image_file->code_section_size, GFP_USER);
	if (tv->enc->seg[0].enc_data == NULL){
		tp_err("Failed to allocate tp section");
		return ;
	}

	memcpy(tv->enc->seg[0].enc_data , image_file->tp_section,image_file->code_section_size);
	memcpy(&tv->enc->seg[0].size,tv->enc->seg[0].enc_data + 0x24,sizeof(int));

	err = create_hyp_mappings(tv->enc->seg[0].enc_data,
 			tv->enc->seg[0].enc_data + image_file->code_section_size);

	if (err){
			tp_err(" failed to map tp_section\n");
			return;
	}

	tp_err("tp section "
			"mapped start %p  size %d but section size is %d\n",
			tv->enc->seg[0].enc_data,
			(int)image_file->code_section_size,
			tv->enc->seg[0].size);

	addr = kmalloc(sizeof(struct hyp_addr ),GFP_USER);
	addr->addr = (unsigned long)tv->enc->seg[0].enc_data;
	addr->size = image_file->code_section_size;
	list_add(&addr->lst, &tv->hyp_addr_lst);

#define	 __TEST_DECRYPTION_DURATION__
#ifdef __TEST_DECRYPTION_DURATION__
	{
	struct timeval t1,t2;
	int us;

	do_gettimeofday(&t1);
	truly_decrypt(tv);
	do_gettimeofday(&t2);

	us = (t2.tv_sec  - t1.tv_sec )/1000000 + ( t2.tv_usec - t1.tv_usec);
	printk("Decrypt duration is %d\n",us);
	msleep(10);
	}
#endif

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
	struct truly_vm *tv = this_cpu_ptr(&TVM);
	struct hyp_addr* tmp,*tmp2;
	unsigned long is_kernel;

	list_for_each_entry_safe(tmp, tmp2, &tv->hyp_addr_lst, lst) {
		is_kernel = tmp->addr & 0xFFFF000000000000;
		if (is_kernel){
			hyp_user_unmap(tmp->addr, tmp->size);
			tp_info("unmapping tp section %lx\n",tmp->addr);
			kfree((void*)tmp->addr);
		} else {
			unmap_user_space_data(tmp->addr , tmp->size);
		}
		list_del(&tmp->lst);
    	kfree(tmp);
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

#define EL1_EXP_LEVEL 0x1
#define EL1_print if (truly_get_exception_level() == EL1_EXP_LEVEL) printk

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

	if (tv->protected_pgd != truly_get_ttbr0_el1()) {
			return -1;
	}

	enc = (struct encrypt_tvm *) KERN_TO_HYP(tv->enc);
	if ( truly_get_exception_level() == EL1_EXP_LEVEL)
				enc = tv->enc;
	EL1_print ("%s %d %p\n",__func__, __LINE__, enc->seg[0].pad_data);

	if (enc->seg[0].pad_data == NULL) {
		EL1_print ("%s %d\n",__func__, __LINE__);
		enc->seg[0].pad_data = (char *)tv->elr_el2;
	}
	EL1_print ("%s %d\n",__func__, __LINE__);
	pad = enc->seg[0].pad_data;
	tv->brk_count_el2++;
	get_decrypted_key(key);
	lines = enc->seg[0].size/ 4;
	EL1_print ("%s %d\n",__func__, __LINE__);
	extra_offset = (enc->seg[0].size/ 16) * 16;
	extra = enc->seg[0].size - extra_offset;
	EL1_print ("%s %d\n",__func__, __LINE__);
	if ( extra > 0) {
		// backup extra code
		tp_hyp_memcpy(extra_lines, pad +  enc->seg[0].size, sizeof(extra_lines) - extra);
		// pad with zeros
		EL1_print ("%s %d\n",__func__, __LINE__);
		tp_hyp_memset(pad + enc->seg[0].size ,(char)0, sizeof(extra_lines) - extra );
		lines++;
		EL1_print ("%s %d\n",__func__, __LINE__);
	}
	EL1_print ("%s %d\n",__func__, __LINE__);
	d = (char *)KERN_TO_HYP(enc->seg[0].enc_data);
	if ( truly_get_exception_level() == EL1_EXP_LEVEL)
				d = enc->seg[0].enc_data;

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

	/*
	* if the fault was in the padded function
	* we must put back the the command that generated the fault
	* ( for example svc ) and re commence it.
	*/
	if (tv->save_cmd < (unsigned long)pad){
		tv->save_cmd = 0;
	}

	if (tv->save_cmd > ( (unsigned long)pad + enc->seg[0].size) ){
		tv->save_cmd = 0;
	}

	if (tv->save_cmd != 0)
		tp_hyp_memcpy(fault_cmd, (char *)tv->save_cmd, sizeof(fault_cmd));

	for (line = 0 ; line < lines ; line++ ) {
		pad[4*line + 0] = 0x60;
		pad[4*line + 1] = 0x00;
		pad[4*line + 2] = 0x20;
		pad[4*line + 3] = 0xd4;
	}

	if (!tv->save_cmd)
		return 0xAAAAAAAA;
	//
	// Must put back the old command
	//
	tp_hyp_memcpy((char *)tv->save_cmd, fault_cmd, sizeof(fault_cmd));
	return lines;
}
