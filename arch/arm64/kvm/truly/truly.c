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

struct truly_vm tvm;

void	truly_set_vtcr_el2(long vtcr_el2)
{
	asm("msr vctr_el2,%0\n" : "=r"  (vtcr_el2));
}

long truly_get_mfr(void)
{	
	long e = 0;
	asm("mrs %0,id_aa64mmfr0_el1\n" : "=r"  (e));
	return e;
}


void truly_set_vttbr_el2(long vttbr_el2)
{	
	asm("msr vttbr_el2,%0\n" : "=r"  (vttbr_el2));
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

void create_level_zero(struct page* pg, long *addr)
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
        printk("EL2 IPA ranges to %p\n",(void *)( *addr));
}

unsigned long tp_create_pg_tbl(void* cxt) 
{
	struct truly_vm* vm = (struct truly_vm *)cxt;
	long addr = 0;
	long vmid = 0;
	struct page *pg_lvl_zero;

/*
 ips = 1 --> 36bits 64GB
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
       create_level_zero(pg_lvl_zero, &addr);
       vm->vttbr_el2 = page_to_phys(pg_lvl_zero) | ( vmid << 48);
	
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

#define VTCR_EL2_T0SZ_BIT_SHIFT 	0
#define VTCR_EL2_SL0_BIT_SHIFT 		6
#define VTCR_EL2_IRGN0_BIT_SHIFT 	8
#define VTCR_EL2_ORGN0_BIT_SHIFT 	10
#define VTCR_EL2_SH0_BIT_SHIFT 		12
#define VTCR_EL2_TG0_BIT_SHIFT 		14
#define VTCR_EL2_PS_BIT_SHIFT 		16

#define SCTLR_EL2_EE_BIT_SHIFT		25
#define SCTLR_EL2_WXN_BIT_SHIFT		19
#define SCTLR_EL2_I_BIT_SHIFT		12
#define SCTLR_EL2_C_BIT_SHIFT		2
#define SCTLR_EL2_A_BIT_SHIFT		1	
#define SCTLR_EL2_M_BIT_SHIFT		0

/* Hyp Configuration Register (HCR) bits */
#define HCR_ID		(UL(1) << 33)
#define HCR_CD		(UL(1) << 32)
#define HCR_RW_SHIFT	31
#define HCR_RW		(UL(1) << HCR_RW_SHIFT)
#define HCR_TRVM	(UL(1) << 30)
#define HCR_HCD		(UL(1) << 29)
#define HCR_TDZ		(UL(1) << 28)
#define HCR_TGE		(UL(1) << 27)
#define HCR_TVM		(UL(1) << 26)
#define HCR_TTLB	(UL(1) << 25)
#define HCR_TPU		(UL(1) << 24)
#define HCR_TPC		(UL(1) << 23)
#define HCR_TSW		(UL(1) << 22)
#define HCR_TAC		(UL(1) << 21)
#define HCR_TIDCP	(UL(1) << 20)
#define HCR_TSC		(UL(1) << 19)
#define HCR_TID3	(UL(1) << 18)
#define HCR_TID2	(UL(1) << 17)
#define HCR_TID1	(UL(1) << 16)
#define HCR_TID0	(UL(1) << 15)
#define HCR_TWE		(UL(1) << 14)
#define HCR_TWI		(UL(1) << 13)
#define HCR_DC		(UL(1) << 12)
#define HCR_BSU		(3 << 10)
#define HCR_BSU_IS	(UL(1) << 10)
#define HCR_FB		(UL(1) << 9)
#define HCR_VA		(UL(1) << 8)
#define HCR_VI		(UL(1) << 7)
#define HCR_VF		(UL(1) << 6)
#define HCR_AMO		(UL(1) << 5)
#define HCR_IMO		(UL(1) << 4)
#define HCR_FMO		(UL(1) << 3)
#define HCR_PTW		(UL(1) << 2)
#define HCR_SWIO	(UL(1) << 1)
#define HCR_VM		(UL(1) << 0)

// D-2142
void make_vtcr_el2(void)
{
	long vtcr_el2_t0sz;
	long vtcr_el2_sl0;
	long vtcr_el2_irgn0;
	long vtcr_el2_orgn0;
	long vtcr_el2_sh0;
	long vtcr_el2_tg0;
	long vtcr_el2_ps;

	vtcr_el2_t0sz   = (truly_get_tcr_el1() & 0b111111);
	vtcr_el2_sl0    = 0b10; // start at level 0.  D.2143
	vtcr_el2_irgn0  = 0;
	vtcr_el2_orgn0  = 0;
	vtcr_el2_sh0    = 0;
 	vtcr_el2_tg0    = (truly_get_tcr_el1() & 0xc000) >> 14;
	vtcr_el2_ps     = (truly_get_tcr_el1() & 0x700000000 ) >> 32;

	tvm.vtcr_el2 =  ( vtcr_el2_t0sz   << VTCR_EL2_T0SZ_BIT_SHIFT  ) |
			( vtcr_el2_sl0    << VTCR_EL2_SL0_BIT_SHIFT   ) |
 		 	( vtcr_el2_irgn0  << VTCR_EL2_IRGN0_BIT_SHIFT ) | 	
			( vtcr_el2_orgn0  << VTCR_EL2_ORGN0_BIT_SHIFT ) |
			( vtcr_el2_sh0    << VTCR_EL2_SH0_BIT_SHIFT   ) |	
			( vtcr_el2_tg0    << VTCR_EL2_TG0_BIT_SHIFT   ) |		
			( vtcr_el2_ps	  << VTCR_EL2_PS_BIT_SHIFT); 		
	
}

void make_sctlr_el2(void)
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
	sctlr_el2_A = 0; // do not do aligment check    
	sctlr_el2_M = 1; // enable MMU

 	tvm.sctlr_el2 = ( sctlr_el2_EE  << SCTLR_EL2_EE_BIT_SHIFT  ) |
			( sctlr_el2_WXN << SCTLR_EL2_WXN_BIT_SHIFT ) |
			( sctlr_el2_I   << SCTLR_EL2_I_BIT_SHIFT   ) |
			( sctlr_el2_C   << SCTLR_EL2_C_BIT_SHIFT   ) |		
			( sctlr_el2_A   << SCTLR_EL2_A_BIT_SHIFT   ) |		      
			( sctlr_el2_M   << SCTLR_EL2_M_BIT_SHIFT );		

}


//
// To be used in hyp mode only
//
int truly_test_vttbr(void *cxt)
{
	// take an arbitary pointer
//	void *p = cxt;
	//
	// copy the main table to vttbr_el2
	make_vtcr_el2();
	make_sctlr_el2();	

	truly_set_vttbr_el2(tvm.vttbr_el2);
	truly_set_vtcr_el2(tvm.vtcr_el2);
	truly_set_sctlr_el2(tvm.sctlr_el2);
	// page 5295
	// should enable DC or VM to have a second stage 
	// translationa
	tvm.hcr_el2 = truly_get_hcr_el2() | HCR_RW | HCR_DC;
	// turn on vm
	truly_set_hcr_el2(tvm.hcr_el2);

	return 767;
}
EXPORT_SYMBOL_GPL(truly_test_vttbr);

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

       id_aa64mmfr0_el1 = truly_get_mfr();
       tcr_el1 = truly_get_tcr_el1(); 

       t0sz = tcr_el1 &	0b111111;
       t1sz = (tcr_el1  >> 16) & 0b111111;
       ips  = (tcr_el1  >> 32) & 0b111;

       pa_range =  id_aa64mmfr0_el1 & 0b1111;
       tp_info("t0sz = %d t1sz=%d ips=%d PARnage=%d\n", 
			t0sz, t1sz, ips, pa_range);

// level 0  
		

/*
  vtcr_el2.t0sz = ??? what is the translation table size
*/
      

       tp_create_pg_tbl(&tvm) ;
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

	return 0;
}

EXPORT_SYMBOL_GPL(truly_get_sctlr_el1);
EXPORT_SYMBOL_GPL(truly_get_sctlr_el2);
EXPORT_SYMBOL_GPL(truly_get_tcr_el2);
EXPORT_SYMBOL_GPL(truly_get_tcr_el1);
EXPORT_SYMBOL_GPL(truly_set_tcr_el2);
EXPORT_SYMBOL_GPL(truly_exec_el1);

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

