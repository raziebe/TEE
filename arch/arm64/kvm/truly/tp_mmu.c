/*
 * tp_mmu.c
 *
 *  Created on: Nov 9, 2017
 *      Author: raz
 */
#include <linux/module.h>
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
#include <asm/pgalloc.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/personality.h> // for stack flags
#include <linux/truly.h>
#include <linux/delay.h>
#include <linux/truly.h>
#include <linux/tp_mmu.h>

#include "tp_types.h"
#include "linux_kernfile.h"
#include "funcCipherStruct.h"
#include "elf.h"
#include "exec_prot_db_linux.h"
#include "ImageFile.h"
#include "ImageManager.h"


#define VM_STCK_FLAGS ( VM_READ | VM_WRITE | VM_MAYWRITE | VM_MAYREAD | VM_MAYEXEC | VM_GROWSDOWN | VM_ACCOUNT)

void tp_map_vmas(struct _IMAGE_FILE* image_file)
{
        struct vm_area_struct* vma = current->mm->mmap;

        for (;vma ; vma = vma->vm_next) {

        	if (is_addr_mapped(vma->vm_start, get_tvm())){
        		tp_debug("%s: %lx already mapped\n",
        				__func__,vma->vm_start);
        		continue;
        	}
        	if (vma->vm_flags & VM_EXEC) {
                tp_debug("Truly mapping executable at %lx %lx\n",
                			vma->vm_start,
							vma->vm_flags);
        		vma_map_hyp(vma);
                continue;
        	}

        	if (vma->vm_flags == VM_STACK_FLAGS) {
        		tp_debug("Truly mapping stack at %lx\n",
        				vma->vm_end - PAGE_SIZE);
        		map_user_space_data((void *)(vma->vm_end - PAGE_SIZE), PAGE_SIZE);
        	}
        }
}


void vma_map_hyp(struct vm_area_struct* vma)
{
	struct truly_vm *tv;
	int vma_size;
	tv = get_tvm();

	vma_size = (int) ( (long) vma->vm_end - (long)vma->vm_start);

	if (tv->enc->seg[0].pad_data == NULL) {
		tv->enc->seg[0].pad_data = (char *)vma->vm_start + tv->enc->seg[0].pad_func_offset;
		tp_debug("Mapping Padded [%p...%p] in  [ 0x%lx...0x%lx]\n",
			tv->enc->seg[0].pad_data,
			tv->enc->seg[0].pad_data + tv->enc->seg[0].size ,
			 vma->vm_start,  vma->vm_start + vma_size );

		map_user_space_data((void *)tv->enc->seg[0].pad_data,
		                       tv->enc->seg[0].size);
		return;
	}
	map_user_space_data((void *)vma->vm_start , PAGE_SIZE);
}

void unmap_user_space_data(unsigned long umem,int size)
{
	hyp_user_unmap(umem,  size,1);
	tp_debug("pid %d unmapped %lx \n", current->pid, umem);
}

int mmu_map_vma(unsigned long addr, struct truly_vm *tv)
{
	int size;
    struct vm_area_struct* vma;

    if (current->mm == NULL) {
    	printk("Truly insane: no mm\n");
    	return -1;
    }

    vma = current->mm->mmap;

    if (is_addr_mapped(addr,tv)){
    	tp_debug("%s %lx already mapped\n",__func__,addr);
    	return 0;
    }

    for (;vma ; vma = vma->vm_next) {
    	size = vma->vm_end  -vma->vm_start;
    	if (!(addr >= vma->vm_start && addr <= vma->vm_end) ){
    		continue;
    	}
    	map_user_space_data( (void *)vma->vm_start, size);
    	return 0;
    }
    return (-1);
}

int mmu_map_page(unsigned long addr, struct truly_vm *tv)
{
    struct vm_area_struct* vma;

    vma = current->mm->mmap;

    if (is_addr_mapped(addr,tv)){
    	tp_debug("%s %lx already mapped\n",__func__,addr);
    	return 0;
    }
   // __do_page_fault
    map_user_space_data( (void *)addr, PAGE_SIZE);
    return 0;
}
/*
 *  handle EL2 mmu fault from EL1.
 *
 *  We map the address and them move back to EL2
 *  to complete the run
 */
void el2_mmu_fault_th(void)
{
	struct truly_vm *tv;
	unsigned long flags;

	local_irq_save(flags);
	tv = get_tvm();
	if (tv->far_el2 == 0 && tv->elr_el2 == 0)
		panic("Faulted in an unknown area");

	if (tv->far_el2)
		map_user_space_data((void *)tv->far_el2, 8);
//
// go back to the hyp to restore back to hyp mode
//
	local_irq_restore(flags);
	tp_call_hyp(el2_mmu_fault_bh);
}

struct hyp_addr* tp_get_addr_segment(long addr,struct truly_vm *tv)
{
	struct hyp_addr* tmp;

	list_for_each_entry(tmp,  &tv->hyp_addr_lst,lst) {

		long start = tmp->addr;
		long end = tmp->addr + tmp->size;

		if ( ( addr < end && addr >= start) )
			return tmp;

	}
	return NULL;
}

int is_addr_mapped(long addr,struct truly_vm *tv)
{
	struct hyp_addr* tmp;

	list_for_each_entry(tmp,  &tv->hyp_addr_lst,lst) {

		long start = tmp->addr;
		long end = tmp->addr + tmp->size;

		if ( ( addr < end && addr >= start) )
			return 1;
	}
	return 0;
}

void tp_prepare_process(struct _IMAGE_FILE* image_file)
{
	int err;
	int cpu;
	struct truly_vm *tv;
	struct hyp_addr *addr;
	unsigned long ttbr0_el1;

	tv =  get_tvm();
	INIT_LIST_HEAD(&tv->hyp_addr_lst);

	memset(&tv->enc->seg[0],0x00,sizeof(tv->enc->seg[0]));
	tv->enc->seg[0].enc_data  = kmalloc(image_file->code_section_size, GFP_USER);
	if (tv->enc->seg[0].enc_data == NULL){
		tp_err("Failed to allocate tp section");
		return ;
	}

	memcpy(tv->enc->seg[0].enc_data , image_file->tp_section,image_file->code_section_size);
	memcpy(&tv->enc->seg[0].size,tv->enc->seg[0].enc_data + 0x24,sizeof(int));

	err = create_hyp_mappings(	tv->enc->seg[0].enc_data,
 								tv->enc->seg[0].enc_data + tv->enc->seg[0].size);

	if (err){
			tp_err(" failed to map tp_section\n");
			return;
	}

	addr = kmalloc(sizeof(struct hyp_addr ),GFP_USER);
	addr->addr = (unsigned long)tv->enc->seg[0].enc_data;
	addr->size = tv->enc->seg[0].size;
	list_add(&addr->lst, &tv->hyp_addr_lst);


	tv->enc->seg[0].decrypted_data = kmalloc(tv->enc->seg[0].size, GFP_USER);
	if (tv->enc->seg[0].decrypted_data == NULL) {
		tp_err(" failed to allocate decrypted section\n");
		return;
	}

	memset(tv->enc->seg[0].decrypted_data, 0x00, tv->enc->seg[0].size);
	err = create_hyp_mappings(	tv->enc->seg[0].decrypted_data,
 								tv->enc->seg[0].decrypted_data + tv->enc->seg[0].size);

	if (err){
			tp_err(" failed to map the decrypted data\n");
			return;
	}
/*
 * To do: protect the data from access from EL1 & EL0
*/
	addr = kmalloc(sizeof(struct hyp_addr ),GFP_USER);
	addr->addr = (unsigned long)tv->enc->seg[0].decrypted_data;
	addr->size = tv->enc->seg[0].size;
	list_add(&addr->lst, &tv->hyp_addr_lst);

	tp_info("Kernel Copy of encrypted section start 0x%lx size %d\n"
			"Kernel copy of decrypted section start 0x%lx\n"
			"section size is %d\n",
			KERN_TO_HYP(tv->enc->seg[0].enc_data),
			(int)image_file->code_section_size,
			KERN_TO_HYP(tv->enc->seg[0].decrypted_data),
			tv->enc->seg[0].size);

	preempt_disable();
	ttbr0_el1 = truly_get_ttbr0_el1();
	preempt_enable();

	for_each_possible_cpu(cpu) {
			struct truly_vm *tvm = get_tvm_per_cpu(cpu);
			if (tvm != tv) {
				tvm->hyp_addr_lst = tv->hyp_addr_lst;
				tvm->enc = tv->enc;
			}
			tvm->protected_pgd = ttbr0_el1;
			tvm->tv_flags = TVM_PROCESS_INIT | TVM_SHOULD_DECRYPT;
			tvm->save_cmd = 0;
			tvm->first_lr = 0;
			tvm->brk_count_el2 = 0;
			tvm->copy_time = 0;
			tvm->decrypt_time = 0;
			tvm->elr_el2 = 0;
			tvm->far_el2 = 0;
			tvm->first_lr = 0;
			tvm->enc->seg[0] = tv->enc->seg[0];
			mb();
	}
}


#define PAGE_HYP_USER	( PROT_DEFAULT  | PTE_ATTRINDX(0) ) // not shared,
extern int __create_hyp_mappings(pgd_t *pgdp,
				 unsigned long start, unsigned long end,
				 unsigned long pfn, pgprot_t prot);
extern pgd_t *hyp_pgd;
/**
 * create_hyp_user_mappings - duplicate a user virtual address range in Hyp mode
 * @from:	The virtual kernel start address of the range
 * @to:		The virtual kernel end address of the range (exclusive)
 *
 * The same virtual address as the kernel virtual address is also used
 * in Hyp-mode mapping (modulo HYP_PAGE_OFFSET) to the same underlying
 * physical pages.
 */
int create_hyp_user_mappings(void *from, void *to)
{
	unsigned long virt_addr;
	unsigned long fr = (unsigned long)from;
	unsigned long start = USER_TO_HYP((unsigned long)from);
	unsigned long end = USER_TO_HYP((unsigned long)to);


	start = start & PAGE_MASK;
	end = PAGE_ALIGN(end);

	for (virt_addr = start; virt_addr < end; virt_addr += PAGE_SIZE,fr += PAGE_SIZE) {
		int err;
		unsigned long pfn;

		pfn = kvm_uaddr_to_pfn(fr);
		if (pfn <= 0)
			continue;

		err = __create_hyp_mappings(hyp_pgd, virt_addr,
					    virt_addr + PAGE_SIZE,
					    pfn,
						PAGE_HYP);
		if (err) {
			printk("TP: Failed to map %p\n",(void *)virt_addr);
			return err;
		}
	}

	return 0;
}

unsigned long kvm_uaddr_to_pfn(unsigned long uaddr)
{
	unsigned long pfn;
	struct page *pages[1];
	int nr;

	nr = get_user_pages_fast(uaddr,1, 0, (struct page **)&pages);
	if (nr <= 0){
	       printk("TP: INSANE: failed to get user pages %p\n",(void *)uaddr);
	       return 0x00;
	}
	pfn = page_to_pfn(pages[0]);
	page_cache_release(pages[0]);
	return pfn;
}

/*
 * Called from execve context.
 * Map the user
 */
void map_user_space_data(void *umem,int size)
{
	int err;
	struct hyp_addr* tmp;
	struct truly_vm *tv;
	struct hyp_addr* addr;
	unsigned long end_addr;
	unsigned long aligned_addr;

	aligned_addr = (unsigned long)umem & PAGE_MASK;
	end_addr = (unsigned long)umem + size;
	tv = get_tvm();
//
	tmp  = tp_get_addr_segment(aligned_addr ,tv);
	if (!tmp)
		goto map;
//
	if ( end_addr < ( tmp->addr + tmp->size ) ){
		tp_debug("%s Truly Found %lx in %lx Size=%d\n",
				__func__,
				end_addr, tmp->addr, tmp->size);
		return;
	}
	printk("XXXXXXXXXXXXXXXXXXXXXXXXX %lx\n",end_addr);

map:
	err = create_hyp_user_mappings(umem, umem + size);
	if (err){
			tp_err(" failed to map ttbr0_el2\n");
			return;
	}

	addr = kmalloc(sizeof(struct hyp_addr ), GFP_ATOMIC);
	addr->addr = (unsigned long)umem & PAGE_MASK;
	addr->size = PAGE_ALIGN((unsigned long)umem + size) - addr->addr;

//	mutex_lock(&tv->sync);
	list_add(&addr->lst, &tv->hyp_addr_lst);
//	mutex_unlock(&tv->sync);

	tp_info("pid %d user mapped real (%p size=%d) in [%lx,%lx] size=%d\n",
			current->pid,umem ,size, addr->addr, addr->addr + addr->size ,addr->size );


}
//
// for any process identified as
//
void tp_unmmap_region(unsigned long start, size_t len)
{
	struct truly_vm *tv = get_tvm();
	struct hyp_addr *tmp;
	unsigned long is_kernel;
	unsigned long end,hyp_end;

	tmp = tp_get_addr_segment(start ,tv);
	if (!tmp){
//		printk("%s %lx is not found\n",__func__,start);
		return;
	}

	is_kernel = tmp->addr & 0xFFFF000000000000;
	if (is_kernel)
		panic("INSNAE . Tried to release a kernel address\n");

	end = start + len;
	hyp_end = tmp->addr + tmp->size;

	tp_info("Truly %lx,%zd ... %ld ,%d\n",start, len, tmp->addr,tmp->size);
//
// All permutations are possible
//
	if (start == tmp->addr && end == hyp_end) {
		unmap_user_space_data(tmp->addr , tmp->size);
		list_del(&tmp->lst);
		kfree(tmp);
		tp_debug("Found addr %ld fully\n", tmp->addr );
		return;
	}

	if (start == tmp->addr && (tmp->size > len)){
		unmap_user_space_data(tmp->addr , len);
		tmp->addr = tmp->addr + len;
		tmp->size = tmp->size - len;
		return;
	}

// segment is bigger
	if (start == tmp->addr && (tmp->size < len)){
		unmap_user_space_data(tmp->addr , tmp->size);
		tmp->addr = tmp->addr + tmp->size;
		tmp->size = len - tmp->size;
		return;
	}

// segment starts inside and crosses
	if (start > tmp->addr && (tmp->size < len)){
		unmap_user_space_data(tmp->addr , tmp->size);
		tmp->addr = tmp->addr + tmp->size;
		tmp->size = len - tmp->size;
		return;
	}

// aiee a split case.
	if (start > tmp->addr && (len < tmp->size)){
		unmap_user_space_data(start , len);
		tmp->size = tmp->size- len;
		printk("TURLY: BUG HERE. MUST SPLIT semgent\n");
		return;
	}
}



//
// for any process identified as
//
void tp_unmmap_handler(struct task_struct* task)
{
	struct truly_vm *tv = get_tvm();
	struct hyp_addr* tmp,*tmp2;
	unsigned long is_kernel;

	list_for_each_entry_safe(tmp, tmp2, &tv->hyp_addr_lst, lst) {
		is_kernel = tmp->addr & 0xFFFF000000000000;
		if (is_kernel){
			hyp_user_unmap(tmp->addr, tmp->size, 0);
			tp_info("unmapping tp section %lx\n",tmp->addr);
			kfree((void*)tmp->addr);
		} else {
			unmap_user_space_data(tmp->addr , tmp->size);
		}
		list_del(&tmp->lst);
    	kfree(tmp);
	}
}


/* **************************************** IPA *************************************************** */


//
// alloc 512 * 4096  = 2MB
//
void create_level_three(struct page *pg, long *addr)
{
	int i;
	long *l3_descriptor;

	l3_descriptor = (long *) kmap(pg);
	if (l3_descriptor == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}

	for (i = 0; i < PAGE_SIZE / sizeof(long long); i++) {
		/*
		 * see page 1781 for details
		 */
		l3_descriptor[i] = (DESC_AF) |
			(0b11 << DESC_SHREABILITY_SHIFT) |
			(0b11 << DESC_S2AP_SHIFT) | (0b1111 << 2) |	/* leave stage 1 unchanged see 1795 */
		   	 DESC_TABLE_BIT | DESC_VALID_BIT | (*addr);

		(*addr) += PAGE_SIZE;
	}
	kunmap(pg);
}

// 1GB
void create_level_two(struct page *pg, long *addr)
{
	int i;
	long *l2_descriptor;
	struct page *pg_lvl_three;

	l2_descriptor = (long *) kmap(pg);
	if (l2_descriptor == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}

	pg_lvl_three = alloc_pages(GFP_KERNEL | __GFP_ZERO, 9);
	if (pg_lvl_three == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	for (i = 0; i < PAGE_SIZE / (sizeof(long)); i++) {
		// fill an entire 2MB of mappings
		create_level_three(pg_lvl_three + i, addr);
		// calc the entry of this table
		l2_descriptor[i] =
		    (page_to_phys(pg_lvl_three + i)) | DESC_TABLE_BIT |
		    DESC_VALID_BIT;

		//tp_info("L2 IPA %lx\n", l2_descriptor[i]);
	}

	kunmap(pg);
}

void create_level_one(struct page *pg, long *addr)
{
	int i;
	long *l1_descriptor;
	struct page *pg_lvl_two;

	l1_descriptor = (long *) kmap(pg);
	if (l1_descriptor == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}

	pg_lvl_two = alloc_pages(GFP_KERNEL | __GFP_ZERO, 3);
	if (pg_lvl_two == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	for (i = 0; i < 8; i++) {
		get_page(pg_lvl_two + i);
		create_level_two(pg_lvl_two + i, addr);
		l1_descriptor[i] =
		    (page_to_phys(pg_lvl_two + i)) | DESC_TABLE_BIT |
		    DESC_VALID_BIT;

	}
	kunmap(pg);
}

void create_level_zero(struct truly_vm *tvm, struct page *pg, long *addr)
{
	struct page *pg_lvl_one;
	long *l0_descriptor;;

	pg_lvl_one = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (pg_lvl_one == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	get_page(pg_lvl_one);
	create_level_one(pg_lvl_one, addr);

	l0_descriptor = (long *) kmap(pg);
	if (l0_descriptor == NULL) {
		printk("%s desc NULL\n", __func__);
		return;
	}

	memset(l0_descriptor, 0x00, PAGE_SIZE);

	l0_descriptor[0] =
	    (page_to_phys(pg_lvl_one)) | DESC_TABLE_BIT | DESC_VALID_BIT;

	tvm->pg_lvl_one = (void *) pg_lvl_one;

	tp_info("L0 IPA %lx\n", l0_descriptor[0]);

	kunmap(pg);

}

void tp_create_pg_tbl(void *cxt)
{
	struct truly_vm *tvm = (struct truly_vm *) cxt;
	long addr = 0;
	long vmid = 012;
	struct page *pg_lvl_zero;
	int starting_level = 1;
/*
 tosz = 25 --> 39bits 64GB
	0-11
2       12-20   :512 * 4096 = 2MB per entry
1	21-29	: 512 * 2MB = per page
0	30-35 : 2^5 entries	, each points to 32 pages in level 1
 	pa range = 1 --> 36 bits 64GB

*/
	pg_lvl_zero = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (pg_lvl_zero == NULL) {
		printk("%s alloc page NULL\n", __func__);
		return;
	}

	get_page(pg_lvl_zero);
	create_level_zero(tvm, pg_lvl_zero, &addr);

	if (starting_level == 0)
		tvm->vttbr_el2 = page_to_phys(pg_lvl_zero) | (vmid << 48);
	else
		tvm->vttbr_el2 =
		    page_to_phys((struct page *) tvm->pg_lvl_one) | (vmid
								     <<
								     48);
}


// D-2142
void make_vtcr_el2(struct truly_vm *tvm)
{
	long vtcr_el2_t0sz;
	long vtcr_el2_sl0;
	long vtcr_el2_irgn0;
	long vtcr_el2_orgn0;
	long vtcr_el2_sh0;
	long vtcr_el2_tg0;
	long vtcr_el2_ps;

	vtcr_el2_t0sz = truly_get_tcr_el1() & 0b111111;
	vtcr_el2_sl0 = 0b01;	//IMPORTANT start at level 1.  D.2143 + D4.1746
	vtcr_el2_irgn0 = 0b1;
	vtcr_el2_orgn0 = 0b1;
	vtcr_el2_sh0 = 0b11;	// inner sharable
	vtcr_el2_tg0 = (truly_get_tcr_el1() & 0xc000) >> 14;
	vtcr_el2_ps = (truly_get_tcr_el1() & 0x700000000) >> 32;

	tvm->vtcr_el2 = (vtcr_el2_t0sz) |
	    (vtcr_el2_sl0 << VTCR_EL2_SL0_BIT_SHIFT) |
	    (vtcr_el2_irgn0 << VTCR_EL2_IRGN0_BIT_SHIFT) |
	    (vtcr_el2_orgn0 << VTCR_EL2_ORGN0_BIT_SHIFT) |
	    (vtcr_el2_sh0 << VTCR_EL2_SH0_BIT_SHIFT) |
	    (vtcr_el2_tg0 << VTCR_EL2_TG0_BIT_SHIFT) |
	    (vtcr_el2_ps << VTCR_EL2_PS_BIT_SHIFT);

}
