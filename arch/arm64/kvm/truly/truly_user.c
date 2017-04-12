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

	for_each_possible_cpu(cpu) {
			tv = &per_cpu(TVM, cpu);
			tv->protected_pid = current->pid;
	}

	tv = this_cpu_ptr(&TVM);
	tv->enc->seg[0].data = image_file->tp_section;
	tv->enc->seg[0].size = image_file->code_section_size;

 	err = create_hyp_mappings(tv->enc->seg[0].data,
 			tv->enc->seg[0].data + tv->enc->seg[0].size);

	if (err){
			tp_err(" failed to map tp_section\n");
			return;
	}

	tp_err("pid %d tp section mapped %p %d bytes\n",
			current->pid,
			tv->enc->seg[0].data,
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

int tp_is_protected(pid_t pid)
{
	struct truly_vm *tv = this_cpu_ptr(&TVM);
	return tv->protected_pid == pid;
}

void tp_unmark_protected(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
			struct truly_vm *tv = this_cpu_ptr(&TVM);
			tv->protected_pid = 0;
	}
}

#include "AesC.h"

void __hyp_text truly_decrypt(struct truly_vm *tv)
{
	int line = 0,lines = 0;
	char *d;
	int data_offset = 60;
	char* pad;
	UCHAR key[16+1] = {0};
	struct encrypt_tvm *enc;


	pad = (char *)tv->elr_el2;
	enc = (struct encrypt_tvm *) KERN_TO_HYP(tv->enc);

//	tv->brk_count_el2++;

	get_decrypted_key(key);
//	lines = ( enc->seg[0].size - 60)/ 4;



//	for (line = 0 ; line < lines ; line += 4 ) {
		d = (char *)KERN_TO_HYP(enc->seg[0].data);
		AESSW_Enc128( enc, d + data_offset, pad, 1 ,key);

		data_offset += 16;
//	}

}
