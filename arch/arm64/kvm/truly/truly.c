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

static struct truly_vm __percpu *tvm;

struct truly_vm* get_tvm(void)
{
		return this_cpu_ptr(tvm);
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



unsigned long truly_test_code_map(void* cxt) 
{
	struct truly_vm *vm =  (struct truly_vm *)cxt;
	vm->debug = 1212;
	return 4444;
}
EXPORT_SYMBOL_GPL(truly_test_code_map);

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

	for (i = 0 ; i < PAGE_SIZE/sizeof(long); i++){
		//
		// see page 1781 for details
		//
       		l3_descriptor[i] =  ( (*addr) << 12) | ( 0b01 << DESC_MEMATTR_SHIFT )| 
			DESC_VALID_BIT | ( 0b11 << DESC_S2AP_SHIFT /* RW */ ) | ( 0b01 << DESC_SHREABILITY_SHIFT );

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
		l2_descriptor[i] = (page_to_phys( pg_lvl_three + i ) << 12) | DESC_TABLE_BIT |
				( 0b01 << DESC_MEMATTR_SHIFT ) | DESC_VALID_BIT |
					( 0b11 << DESC_S2AP_SHIFT )  | ( 0b01 << DESC_SHREABILITY_SHIFT );
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
 
       pg_lvl_two = alloc_pages(GFP_KERNEL | __GFP_ZERO, 1);
       if (pg_lvl_two == NULL){
    	   	   printk("%s alloc page NULL\n",__func__);
    	   	   return;
       }
       	
       for (i = 0 ; i  < 2 ; i++) {
 
		create_level_two(pg_lvl_two + i , addr);

		l1_descriptor[i] = (page_to_phys(pg_lvl_two + i) << 12) | DESC_TABLE_BIT | 
			( 0b01 << DESC_MEMATTR_SHIFT ) | DESC_VALID_BIT | ( 0b11 << DESC_S2AP_SHIFT )  | ( 0b01 << DESC_SHREABILITY_SHIFT );

       }
       kunmap(pg);
}

void create_level_zero(struct truly_vm* tvm, struct page* pg, long *addr)
{
       struct page *pg_lvl_one;
       long* l0_descriptor;

       pg_lvl_one = alloc_page(GFP_KERNEL | __GFP_ZERO);
       if (pg_lvl_one == NULL){
    	   printk("%s alloc page NULL\n",__func__);
    	   return;
       }

       create_level_one(pg_lvl_one, addr);

       l0_descriptor = (long *)kmap(pg);	
       if (l0_descriptor == NULL){
    	   printk("%s desc NULL\n",__func__);
    	   return;
       }
       memset(l0_descriptor, 0x00, PAGE_SIZE);

       l0_descriptor[0] = (page_to_phys(pg_lvl_one) << 12 ) | DESC_TABLE_BIT |
		 ( 0b01 << DESC_MEMATTR_SHIFT ) | DESC_VALID_BIT | ( 0b11 << DESC_S2AP_SHIFT )  | ( 0b01 << DESC_SHREABILITY_SHIFT );

        kunmap(pg);
        tvm->pg_lvl_one = (void *)pg_lvl_one;
        printk("EL2 IPA ranges to %p\n",(void *)( *addr));
}

unsigned long tp_create_pg_tbl(void* cxt) 
{
	struct truly_vm* vm = (struct truly_vm *)cxt;
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
       create_level_zero(tvm, pg_lvl_zero, &addr);
       if (starting_level == 0)
    	   vm->vttbr_el2 = page_to_phys(pg_lvl_zero) | ( vmid << 48);
       else
    	   vm->vttbr_el2 = page_to_phys( (struct page *)tvm->pg_lvl_one) | ( vmid << 48);

       return vm->vttbr_el2;
}


long truly_get_mem_regs(void *cxt)
{
	struct truly_vm* vm;
	
	vm = (struct truly_vm *)cxt;

	vm->id_aa64mmfr0_el1 = truly_get_mfr();
	vm->tcr_el1 = truly_get_tcr_el1();
	return 0;
}
EXPORT_SYMBOL_GPL(truly_get_mem_regs);


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
	vtcr_el2_sl0    = 0b01; // start at level 1.  D.2143 + D4.1746
	vtcr_el2_irgn0  = 0;
	vtcr_el2_orgn0  = 0;
	vtcr_el2_sh0    = 0;
 	vtcr_el2_tg0    = (truly_get_tcr_el1() & 0xc000) >> 14;
	vtcr_el2_ps     = (truly_get_tcr_el1() & 0x700000000 ) >> 32;

	tvm->vtcr_el2 =  ( vtcr_el2_t0sz  ) |
			( vtcr_el2_sl0    << VTCR_EL2_SL0_BIT_SHIFT   ) |
 		 	( vtcr_el2_irgn0  << VTCR_EL2_IRGN0_BIT_SHIFT ) | 	
			( vtcr_el2_orgn0  << VTCR_EL2_ORGN0_BIT_SHIFT ) |
			( vtcr_el2_sh0    << VTCR_EL2_SH0_BIT_SHIFT   ) |	
			( vtcr_el2_tg0    << VTCR_EL2_TG0_BIT_SHIFT   ) |		
			( vtcr_el2_ps	  << VTCR_EL2_PS_BIT_SHIFT); 		

	printk("RAZ tcr_el1.t0sz=%ld %ld %ld\n",
			truly_get_tcr_el1() & 0b111111 ,
			vtcr_el2_t0sz ,
			tvm->vtcr_el2 & 0b111111  );
}

void make_sctlr_el2(struct truly_vm *tvm)
{
	long sctlr_el2_EE;
	long sctlr_el2_WXN;
	long sctlr_el2_I;
	long sctlr_el2_C;
	long sctlr_el2_A;    
	long sctlr_el2_M;


	sctlr_el2_EE = ( truly_get_sctlr_el1() & 0x2000000 ) >> 25;
	sctlr_el2_WXN = 0; // no effect
	sctlr_el2_I =  0; // instructions non-cachable
	sctlr_el2_C = 0; // data non-cacheable
	sctlr_el2_A = 0; // do not do alignment check
	sctlr_el2_M = 1; // enable MMU

 	tvm->sctlr_el2 = ( sctlr_el2_EE  << SCTLR_EL2_EE_BIT_SHIFT  ) |
			( sctlr_el2_WXN << SCTLR_EL2_WXN_BIT_SHIFT ) |
			( sctlr_el2_I   << SCTLR_EL2_I_BIT_SHIFT   ) |
			( sctlr_el2_C   << SCTLR_EL2_C_BIT_SHIFT   ) |		
			( sctlr_el2_A   << SCTLR_EL2_A_BIT_SHIFT   ) |		      
			( sctlr_el2_M   << SCTLR_EL2_M_BIT_SHIFT );		

}

 void make_hstr_el2(struct truly_vm *tvm)
{
	tvm->hstr_el2  =  1 << 15 ; // Trap CP15 Cr=15
}

void make_hcr_el2(struct truly_vm *tvm)
{
	tvm->hcr_el2 = HCR_GUEST_FLAGS;
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
       int cpu;
       int pa_range;
       long id_aa64mmfr0_el1;

       id_aa64mmfr0_el1 = truly_get_mfr();
       tcr_el1 = truly_get_tcr_el1(); 

       t0sz = tcr_el1 &	0b111111;
       t1sz = (tcr_el1  >> 16) & 0b111111;
       ips  = (tcr_el1  >> 32) & 0b111;

       pa_range =  id_aa64mmfr0_el1 & 0b1111;
       tp_info("t0sz = %d t1sz=%d ips=%d PARnage=%d\n", 
			t0sz, t1sz, ips, pa_range);

       tp_info("Memory Layout code start=%p,%p\n "
                       "code end =%p,%p\n"
                       " size of code=%d\n"
                       "data start = %p,%p\n"
                       " end = %p,%p,\n" "size of data=%d\n",
                       (void *)_text, (void*)virt_to_phys(_text),
                       (void *)_end, (void *)virt_to_phys(_etext),
                       (int)(virt_to_phys(_end) - virt_to_phys(_text)),
                       (void*)_sdata, (void *)virt_to_phys(_sdata),
                       (void*)_end, (void *)virt_to_phys(_end-1),
                       (int)(virt_to_phys(_end-1) - virt_to_phys(_sdata) ) );


       tvm = alloc_percpu(struct truly_vm);
       if (!tvm) {
           tp_info("Cannot allocate Truly VM\n");
           return -1;
     }

     for_each_possible_cpu(cpu) {

    	 	  int create_hyp_mappings(void*,void*);
    	 	 int err;
    	 	  struct truly_vm *_tvm;

               _tvm = per_cpu_ptr(tvm, cpu);

               err = create_hyp_mappings(_tvm, _tvm + 1);
               if (err){
            	   	   tp_err("Failed to map tvm");
            	   	   return -1;
               }
     }
     tp_create_pg_tbl(get_tvm()) ;

     make_vtcr_el2(get_tvm());
     make_sctlr_el2(get_tvm());
     make_hcr_el2(get_tvm());


	return 0;
}

EXPORT_SYMBOL_GPL(truly_set_vttbr_el2);
EXPORT_SYMBOL_GPL(get_tvm);
EXPORT_SYMBOL_GPL(truly_run_vm);
EXPORT_SYMBOL_GPL(truly_get_sctlr_el1);
EXPORT_SYMBOL_GPL(truly_get_sctlr_el2);
EXPORT_SYMBOL_GPL(truly_get_tcr_el2);
EXPORT_SYMBOL_GPL(truly_get_tcr_el1);
EXPORT_SYMBOL_GPL(truly_set_tcr_el2);
EXPORT_SYMBOL_GPL(truly_exec_el1);	// VHE support

EXPORT_SYMBOL_GPL(truly_get_ttbr0_el2);
EXPORT_SYMBOL_GPL(truly_get_ttbr1_el2);
EXPORT_SYMBOL_GPL(truly_set_ttbr1_el2);
EXPORT_SYMBOL_GPL(truly_set_ttbr0_el2);

EXPORT_SYMBOL_GPL(truly_set_hcr_el2);
EXPORT_SYMBOL_GPL(truly_has_vhe);
EXPORT_SYMBOL_GPL(truly_get_hcr_el2);
EXPORT_SYMBOL_GPL(truly_get_mdcr_el2);
EXPORT_SYMBOL_GPL(truly_get_vttbr_el2);
EXPORT_SYMBOL_GPL(truly_get_tpidr);
EXPORT_SYMBOL_GPL(truly_set_mdcr_el2);
EXPORT_SYMBOL_GPL(truly_set_tpidr);
EXPORT_SYMBOL_GPL(truly_set_sctlr_el2);
