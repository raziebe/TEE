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

DECLARE_PER_CPU(struct truly_vm, TVM);

/*
 * Called from execve context.
 * Map the user
 */
void map_user_space_data(void *umem,int size,int encrypted)
{
	int cpu;
	int err;
	void *kaddr = NULL;
    int nr_pages = (size >> PAGE_SHIFT) + 1;
    struct page *pages[nr_pages];
    int nr;
    int i = 0;

     nr = get_user_pages(current,
    		 	 	 	 current->mm,
						 (unsigned long)umem,
						 nr_pages, 0,     /* write */
                         0,  /* force */
                         (struct page **)&pages, 0);
    if (nr == 0){
    	tp_info("INSANE: failed to get user pages");
    	return;
    }
    tp_err(" pid %d size=%d nr %d %d\n",current->pid,size,nr,nr_pages);
	//
	// break the address to pages and map
	// each page to hypervisor

	for ( i = 0; i < nr ; i++)  {
		kaddr = kmap(pages[i]);
		err = create_hyp_mappings(kaddr, kaddr + PAGE_SIZE);
		if (err){
				tp_err(" failed to map ttbr0_el2");
				return;
		}
		tp_err("kaddr %p %p\n",kaddr,(char *)umem + i*4096);
	}


// bug here. I assume a single page. Must must change to dynamic size
	for_each_possible_cpu(cpu) {
		struct truly_vm *tv = this_cpu_ptr(&TVM);

		if (encrypted){
			 	tv->encrypt.uaddr = umem;
			 	tv->encrypt.kaddr = kaddr;
			 	tv->encrypt.size = size;
		} else{
				tv->padd.uaddr = umem;
				tv->padd.kaddr = kaddr;
				tv->padd.size = size;
		}
	}

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
void tp_mmap_handler(void* addr,int len,unsigned long vm_flags)
{
	struct truly_vm *tv = this_cpu_ptr(&TVM);
	 if (current->pid == tv->protected_pid)
		 map_user_space_data(addr, len, 0);
}
