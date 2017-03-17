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

int create_hyp_mappings(void*,void*);
DECLARE_PER_CPU(struct truly_vm, TVM);
DEFINE_PER_CPU(struct truly_vm, TVM);

#define __hyp_text __section(.hyp.text) notrace
#define __hyp	// a marker to show that this code is non-mmu

#define	HYP_TTBR0_EL2_FLAGS_MASK 	0xFFFF000000000FFF
#define HYP_TTBR0_EL2_ADDR_MASK 	0x0000FFFFFFFFF000
#define __hyp_ttbr0_el2_addr(addr)	(HYP_TTBR0_EL2_ADDR_MASK & addr)

//
// Walk on the pages of TTBR0_EL2
// Assume 3 levels
// assume 4096 bytes page size
//
void  __hyp_text truly_walk_on_hyp_pages(struct truly_vm *tvm,unsigned long addr)
{
	unsigned long level_3, level_2,level_1;
	unsigned long hyp_va;
	pgd_t *ttbr0_el2;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	
	tvm->ttbr0_el2 =  tp_call_hyp(tp_get_ttbr0_el2);
	tp_info("Dumping ttbr0_el2 table %p\n",
			(unsigned long *)tvm->ttbr0_el2);

	// first cast to hyp va
	hyp_va = KERN_TO_HYP(addr);
	tp_info("%lx --> %lx\n",addr, hyp_va);

	level_3 = pgd_index(hyp_va);
	level_2 = pmd_index(hyp_va);
	level_1 = pte_index(hyp_va);
	tp_info("l1=%lx l2=%lx l3=%lx\n",level_1,level_2,level_3);
	//
	// while the address is in hyp , to access the tables
	// we need to use kernel virtual address
	//
	ttbr0_el2 = (pgd_t *)tvm->ttbr0_el2;
	ttbr0_el2 =  phys_to_virt( (phys_addr_t ) ttbr0_el2 );
	pgd = ttbr0_el2 + level_3;
	// take pmd 
	pmd = (pmd_t *) __hyp_ttbr0_el2_addr( (unsigned long) *pgd);
	pmd = (pmd_t *) (pmd + level_2); // pmd value
	pmd = phys_to_virt( (phys_addr_t) pmd);
	// take pte
	pte = (pte_t *)__hyp_ttbr0_el2_addr( (unsigned long) pmd);
	pte = (pte_t *) (pte + level_1); // pte value

	printk("truly: TTBR0_EL2=%llx PGD=%llx PMD=%llx PTE=%llx\n",
		(unsigned long long)ttbr0_el2,
		(unsigned long long)*pgd, 
		(unsigned long long) pmd,
		(unsigned long long) pte );
	tp_call_hyp(tp_get_ttbr0_el2);
}

struct truly_vm* get_tvm(void)
{
	return this_cpu_ptr(&TVM);
}

long truly_get_elr_el1(void)
{
	long e;

	asm("mrs  %0, elr_el1\n" : "=r" (e) ) ;
	return e;
}

void truly_set_sp_el1(long e)
{
	asm("msr  sp_el1,%0\n" :  "=r" (e) ) ;
}

void truly_set_elr_el1(long e)
{
	asm("msr  elr_el1,%0\n" :  "=r" (e) ) ;
}

void	truly_set_vtcr_el2(long vtcr_el2)
{
	asm("msr vctr_el2,%0\n" : "=r"  (vtcr_el2));
}

long 	truly_get_mfr(void)
{	
	long e = 0;
	asm("mrs %0,id_aa64mmfr0_el1\n" : "=r"  (e));
	return e;
}

//
// alloc 512 * 4096  = 2MB 
//
void create_level_three(struct page* pg,long* addr)
{
	int i;
    long *l3_descriptor;

	l3_descriptor = (long *)kmap(pg);
    if (l3_descriptor == NULL){
		printk("%s desc NULL\n",__func__);
		return;
    }

	for (i = 0 ; i < PAGE_SIZE/sizeof(long long); i++){
		//
		// see page 1781 for details
		//
       	l3_descriptor[i] =  ( DESC_AF ) |
							( 0b11 << DESC_SHREABILITY_SHIFT ) |
							( 0b11 << DESC_S2AP_SHIFT )|
       						( 0b1111 << 2)  | /* leave stage 1 un-changed see 1795*/
       						  DESC_TABLE_BIT |
							  DESC_VALID_BIT |
							  (*addr) ;

       	(*addr) += PAGE_SIZE;
	}

	kunmap(pg);
}

// 1GB
void create_level_two(struct page *pg, long* addr)
{
       int i;
       long* l2_descriptor;
       struct page* pg_lvl_three;

       l2_descriptor = (long *)kmap(pg);
       if (l2_descriptor == NULL){
    	   printk("%s desc NULL\n",__func__);
    	   return;
       }

       pg_lvl_three = alloc_pages(GFP_KERNEL | __GFP_ZERO , 9);
       if (pg_lvl_three == NULL){
    	   	printk("%s alloc page NULL\n",__func__);
			return;
       }

       for (i = 0 ; i < PAGE_SIZE/(sizeof(long)); i++){
    	   // fill an entire 2MB of mappings
    	   create_level_three(pg_lvl_three + i, addr);
    	   // calc the entry of this table
    	   l2_descriptor[i] = (page_to_phys( pg_lvl_three + i )) | DESC_TABLE_BIT | DESC_VALID_BIT ;

    	   //tp_info("L2 IPA %lx\n", l2_descriptor[i]);
       }

       kunmap(pg);
}

void create_level_one(struct page *pg, long* addr)
{
       int i;
       long* l1_descriptor;
       struct page *pg_lvl_two;

       l1_descriptor = (long *)kmap(pg);	
       if (l1_descriptor == NULL){
    	   	   printk("%s desc NULL\n",__func__);
    	   	   return;
       }
 
       pg_lvl_two = alloc_pages(GFP_KERNEL | __GFP_ZERO, 3);
       if (pg_lvl_two == NULL){
    	   	   printk("%s alloc page NULL\n",__func__);
    	   	   return;
       }
       	
       for (i = 0 ; i  < 8 ; i++) {
    	   get_page(pg_lvl_two + i);
    	   create_level_two(pg_lvl_two + i , addr);

    	   l1_descriptor[i] = (page_to_phys(pg_lvl_two + i)) |   DESC_TABLE_BIT | DESC_VALID_BIT ;

       }
       kunmap(pg);
}

void create_level_zero(struct truly_vm* tvm, struct page* pg, long *addr)
{
       struct page *pg_lvl_one;
       long* l0_descriptor;;

       pg_lvl_one = alloc_page(GFP_KERNEL | __GFP_ZERO);
       if (pg_lvl_one == NULL){
    	   	   printk("%s alloc page NULL\n",__func__);
    	   	   return;
       }

       get_page(pg_lvl_one);
       create_level_one(pg_lvl_one, addr);

       l0_descriptor = (long *)kmap(pg);	
       if (l0_descriptor == NULL){
    	   	   printk("%s desc NULL\n",__func__);
    	   	   return;
       }

       memset(l0_descriptor, 0x00, PAGE_SIZE);

       l0_descriptor[0] = (page_to_phys(pg_lvl_one) ) | DESC_TABLE_BIT | DESC_VALID_BIT;

       tvm->pg_lvl_one = (void *)pg_lvl_one;

       tp_info("L0 IPA %lx\n", l0_descriptor[0]);

       kunmap(pg);

}

unsigned long tp_create_pg_tbl(void* cxt) 
{
	struct truly_vm* tvm = (struct truly_vm *)cxt;
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
      if (pg_lvl_zero == NULL){
    	   printk("%s alloc page NULL\n",__func__);
    	   return 0x00;
      }

      get_page(pg_lvl_zero);
      create_level_zero(tvm, pg_lvl_zero, &addr);

      if (starting_level == 0)
    	   tvm->vttbr_el2 = page_to_phys(pg_lvl_zero) | ( vmid << 48);
       else
    	   tvm->vttbr_el2 = page_to_phys( (struct page *)tvm->pg_lvl_one) | ( vmid << 48);

       return tvm->vttbr_el2;
}


long truly_get_mem_regs(void *cxt)
{
	struct truly_vm* vm;
	
	vm = (struct truly_vm *)cxt;

	vm->id_aa64mmfr0_el1 = truly_get_mfr();
	vm->tcr_el1 = truly_get_tcr_el1();
	return 0;
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

	vtcr_el2_t0sz   = truly_get_tcr_el1() & 0b111111;
	vtcr_el2_sl0    = 0b01; //IMPORTANT start at level 1.  D.2143 + D4.1746
	vtcr_el2_irgn0  = 0b1;
	vtcr_el2_orgn0  = 0b1;
	vtcr_el2_sh0    = 0b11; // inner sharable
 	vtcr_el2_tg0    = (truly_get_tcr_el1() & 0xc000) >> 14;
	vtcr_el2_ps     = (truly_get_tcr_el1() & 0x700000000 ) >> 32;

	tvm->vtcr_el2 =  ( vtcr_el2_t0sz  ) |
			( vtcr_el2_sl0    << VTCR_EL2_SL0_BIT_SHIFT   ) |
 		 	( vtcr_el2_irgn0  << VTCR_EL2_IRGN0_BIT_SHIFT ) | 	
			( vtcr_el2_orgn0  << VTCR_EL2_ORGN0_BIT_SHIFT ) |
			( vtcr_el2_sh0    << VTCR_EL2_SH0_BIT_SHIFT   ) |	
			( vtcr_el2_tg0    << VTCR_EL2_TG0_BIT_SHIFT   ) |		
			( vtcr_el2_ps	  << VTCR_EL2_PS_BIT_SHIFT); 		

}

void make_sctlr_el2(struct truly_vm *tvm)
{
	char sctlr_el2_EE = 0b00;
	char sctlr_el2_M = 0b01; // enable MMU
	char sctlr_el2_SA = 0b1; // SP alignment check
	char sctlr_el2_A = 0b1; //  alignment check


 	tvm->sctlr_el2 =	( sctlr_el2_A   << SCTLR_EL2_A_BIT_SHIFT   ) |
 						( sctlr_el2_SA  << SCTLR_EL2_SA_BIT_SHIFT  ) |
 						( sctlr_el2_EE  << SCTLR_EL2_EE_BIT_SHIFT  ) |
 					    ( sctlr_el2_M   << SCTLR_EL2_M_BIT_SHIFT );

}

 void make_hstr_el2(struct truly_vm *tvm)
{
	tvm->hstr_el2  = 0;// 1 << 15 ; // Trap CP15 Cr=15
}

void make_hcr_el2(struct truly_vm *tvm)
{
	tvm->hcr_el2 = HCR_TRULY_FLAGS;
}

void make_mdcr_el2(struct truly_vm *tvm)
{
	tvm->mdcr_el2  = 0x100;
}

static struct proc_dir_entry *procfs = NULL;

static ssize_t proc_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t* dummy)
{

	return count;
}

static int proc_open(struct inode *inode, struct file *filp)
{
        filp->private_data = (void *)0x1;
        return 0;
}

static ssize_t proc_read(struct file *filp, char __user * page,
                           size_t size, loff_t * off)
{
		ssize_t len = 0;
		int cpu;

        if (filp->private_data == 0x00)
                return 0;
        for_each_possible_cpu(cpu) {
        	struct truly_vm *tv =  &per_cpu(TVM, cpu);
            len += sprintf(page +len, "cpu %d brk count %ld\n",cpu,
                           tv->brk_count_el2);
        }
        filp->private_data = 0x00;
        return len;
}



static struct file_operations proc_ops = {
      .open  = proc_open,
      .read =    proc_read,
      .write =  proc_write,
};


static void init_procfs(void)
{
	procfs = proc_create_data("truly_stats", O_RDWR , NULL, &proc_ops , NULL);
}

/*
 * construct page table
*/
int truly_init(void)
{
     long long tcr_el1;
     int t0sz;
     int t1sz;
     int ips;
     int pa_range;
     long id_aa64mmfr0_el1;
   	 struct truly_vm *_tvm;
   	 int cpu = 0 ;

     id_aa64mmfr0_el1 = truly_get_mfr();
     tcr_el1 = truly_get_tcr_el1();

     t0sz = tcr_el1 &	0b111111;
     t1sz = (tcr_el1  >> 16) & 0b111111;
     ips  = (tcr_el1  >> 32) & 0b111;
     pa_range =  id_aa64mmfr0_el1 & 0b1111;

     _tvm = this_cpu_ptr(&TVM);
     memset(_tvm, 0x00, sizeof(*_tvm));
     tp_create_pg_tbl(_tvm) ;
     make_vtcr_el2(_tvm);
     make_sctlr_el2(_tvm);
     make_hcr_el2(_tvm);
     make_mdcr_el2(_tvm);
     _tvm->temp_page =  kmalloc(4096, GFP_ATOMIC);
      memset(_tvm->temp_page,'a',4096); // marker

     for_each_possible_cpu(cpu) {
    	struct truly_vm *tv =  &per_cpu(TVM, cpu);
    	if (tv != _tvm) {
    		memcpy(tv, _tvm, sizeof(*_tvm));
    	}
     }
/*
     tp_info("HYP_PAGE_OFFSET_SHIFT=%x HYP_PAGE_OFFSET_MASK=%lx HYP_PAGE_OFFSET=%lx PAGE_OFFSET=%lx\n",
    		 (long)HYP_PAGE_OFFSET_SHIFT,
			 (long)HYP_PAGE_OFFSET_MASK,
			 (long)HYP_PAGE_OFFSET,PAGE_OFFSET);
*/
     init_procfs();

     return 0;
}



void truly_map_tvm(void *d)
{
		int err;
		struct truly_vm *tv =  this_cpu_ptr(&TVM);

		if (tv->initialized)
				return;

	   	err = create_hyp_mappings(tv, tv + 1);
	   	if (err){
	   		tp_err("Failed to map tvm");
	   	 } else{
	   		tp_info("Mapped tvm");
	   	}
	   	tp_info("PGDIR_SHIFT =%ld PTRS_PER_PGD =%ld\n", (long)PGDIR_SHIFT , (long)PTRS_PER_PGD);
	   	// map the temp page
	   	err = create_hyp_mappings(tv->temp_page, (char *)tv->temp_page + 4096);
	   	tv->initialized = 1;
	   	mb();
	  	truly_walk_on_hyp_pages(tv, (unsigned long) tv->temp_page);
}

void tp_run_vm(void *x)
{
	long hcr_el2=0;
    struct truly_vm *_tvm = this_cpu_ptr(&TVM);
	unsigned long vbar_el2;
	unsigned long vbar_el2_current = (unsigned long)(KERN_TO_HYP( __truly_vectors ));

	truly_map_tvm(NULL);
	vbar_el2 = truly_get_vectors();
	if (vbar_el2 !=  vbar_el2_current) {
		tp_info("vbar_el2 should restore\n");
		truly_set_vectors(vbar_el2);
	}
    tp_call_hyp(truly_run_vm, _tvm);
    tp_info("Raz Stop Now and look at the ttbr0_el2!");
    hcr_el2 = tp_call_hyp(truly_get_hcr_el2);
}

void truly_smp_run_hyp(void)
{
	on_each_cpu(tp_run_vm , NULL, 0);
}


EXPORT_SYMBOL_GPL(truly_get_vectors);
EXPORT_SYMBOL_GPL(truly_get_mem_regs);
EXPORT_SYMBOL_GPL(truly_smp_run_hyp);
EXPORT_SYMBOL_GPL(truly_get_hcr_el2);
EXPORT_SYMBOL_GPL(tp_call_hyp);
EXPORT_SYMBOL_GPL(truly_init);
EXPORT_SYMBOL_GPL(get_tvm);
