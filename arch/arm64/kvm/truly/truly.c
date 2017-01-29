#include <linux/module.h>
#include <linux/truly.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/highmem.h>

void *v;
int global1=0x43;
static int global2=0x4;
unsigned long truly_test_code_map(void* cxt) 
{
	struct truly_vm* vm = (struct truly_vm *)cxt;
	// We can allocate pages, we cannot map them
//	struct page  *p = alloc_page(GFP_ATOMIC);
	vm->debug = 0x8888; // make a signature
//	printk("xxxxx\n");
	return global1 + global2;
}
EXPORT_SYMBOL_GPL(truly_test_code_map);

long truly_get_mem_regs(void *cxt)
{
	struct truly_vm* vm = (struct truly_vm *)cxt;
	
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
	vm->id_aa64mmfr0_el1 = tp_read_mfr();
	vm->tcr_el1 = tp_read_tcr_el1();
	return 0;
}
EXPORT_SYMBOL_GPL(truly_get_mem_regs);

int truly_init(void)
{
	tp_info("init sucessfully\n");
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


