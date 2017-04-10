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

void tp_mark_protected(int pid)
{
	int cpu;
	for_each_possible_cpu(cpu) {
			struct truly_vm *tv = this_cpu_ptr(&TVM);
			tv->protected_pid = pid;
	}
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

void get_decrypted_key(UCHAR *key)
{
	memcpy(key,"2b7e151628aed2a6abf7158809cf4f3c",32);
}

void __hyp_text truly_debug_decrypt(UCHAR *encrypted,UCHAR* decrypted, int size)
{
	struct truly_vm *tv = this_cpu_ptr(&TVM);
	UCHAR key[32];
	get_decrypted_key(key);
	AESSW_Dec128(tv->enc, encrypted, decrypted, key, 1);

}
