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

int create_hyp_mappings(void *, void *);
int create_hyp_user_mappings(void *,void*);

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
	struct truly_vm *tv = this_cpu_ptr(&TVM);
	 map_user_space_data(addr, len, vm_flags);
}
