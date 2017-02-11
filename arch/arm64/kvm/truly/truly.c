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

long tp_read_tcr_el1(void)
{	
	long e = 0;
	asm("mrs %0,tcr_el1\n" : "=r"  (e));
	return e;
}


long tp_read_mfr(void)
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
       	long *desc;

	desc = (long *)kmap(pg);

	for (i = 0 ; i < PAGE_SIZE/sizeof(long); i++){
		//
		// see page 1781 for details
		//
       		desc[i] =  ( (*addr) << 12) | ( 0b01 << DESC_MEMATTR_SHIFT )| DESC_VALID_BIT | ( 0b11 << DESC_S2AP_SHIFT /* RW */ ) | ( 0b01 << DESC_SHREABILITY_SHIFT );

		*addr += PAGE_SIZE;
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

       pg_lvl_three = alloc_pages(GFP_KERNEL | __GFP_ZERO , 9);
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
 
       pg_lvl_two = alloc_pages(GFP_KERNEL | __GFP_ZERO, 1);
       	
       for (i = 0 ; i  < 2 ; i++) {
 
		create_level_two(pg_lvl_two + i , addr);

		l1_descriptor[i] = (page_to_phys(pg_lvl_two + i) << 12) | DESC_TABLE_BIT | ( 0b01 << DESC_MEMATTR_SHIFT ) | DESC_VALID_BIT | ( 0b11 << DESC_S2AP_SHIFT )  | ( 0b01 << DESC_SHREABILITY_SHIFT );

       }
       kunmap(pg);
}

void create_level_zero(struct page* pg, long *addr)
{
       struct page *pg_lvl_one;
       long* l0_descriptor;

       pg_lvl_one = alloc_page(GFP_KERNEL | __GFP_ZERO);

       create_level_one(pg_lvl_one, addr);

       l0_descriptor = (long *)kmap(pg);	
       memset(l0_descriptor, 0x00, PAGE_SIZE);

       l0_descriptor[0] = (page_to_phys(pg_lvl_one) << 12 ) | DESC_TABLE_BIT | ( 0b01 << DESC_MEMATTR_SHIFT ) | DESC_VALID_BIT | ( 0b11 << DESC_S2AP_SHIFT )  | ( 0b01 << DESC_SHREABILITY_SHIFT );

	kunmap(pg);
        printk("EL2 IPA ranges to %p\n", (void*)addr);
}

unsigned long truly_create_pg_tbl(void* cxt) 
{
	struct truly_vm* vm = (struct truly_vm *)cxt;
	long* addr = 0;
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
       create_level_zero(pg_lvl_zero, addr);
       vm->el2_tbl = pg_lvl_zero;

       return (long)vm->el2_tbl;
}
EXPORT_SYMBOL_GPL(truly_create_pg_tbl);

long truly_get_mem_regs(void *cxt)
{
	struct truly_vm* vm;
	
	vm = (struct truly_vm *)cxt;

	vm->id_aa64mmfr0_el1 = tp_read_mfr();
	vm->tcr_el1 = tp_read_tcr_el1();
	return 0;
}
EXPORT_SYMBOL_GPL(truly_get_mem_regs);

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

       id_aa64mmfr0_el1 = tp_read_mfr();
       tcr_el1 = tp_read_tcr_el1(); 

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
      

       truly_create_pg_tbl(&tvm) ;
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
