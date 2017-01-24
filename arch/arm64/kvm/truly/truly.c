#include <linux/compiler.h>
#include <linux/kvm_host.h>
#include <asm/kvm_mmu.h> 
#include <asm/memory.h>
#include <linux/module.h>
#include <linux/truly.h>
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/irqchip/arm-gic-v3.h>
#include <asm/sections.h>
#include <linux/truly.h>

#define ARM64_WORKAROUND_CLEAN_CACHE            0
#define ARM64_WORKAROUND_DEVICE_LOAD_ACQUIRE    1
#define ARM64_WORKAROUND_845719                 2
#define ARM64_HAS_SYSREG_GIC_CPUIF              3
#define ARM64_HAS_PAN                           4
#define ARM64_HAS_LSE_ATOMICS                   5
#define ARM64_WORKAROUND_CAVIUM_23154           6
#define ARM64_WORKAROUND_834220                 7
#define ARM64_HAS_NO_HW_PREFETCH                8
#define ARM64_HAS_UAO                           9
#define ARM64_ALT_PAN_NOT_UAO                   10
#define ARM64_HAS_VIRT_HOST_EXTN                11
#define ARM64_WORKAROUND_CAVIUM_27456           12
#define ARM64_HAS_32BIT_EL0                     13
#define ARM64_HYP_OFFSET_LOW                    14
#define ARM64_MISMATCHED_CACHE_LINE_SIZE        15

#define HYP_PAGE_OFFSET_HIGH_MASK       ((UL(1) << VA_BITS) - 1)
#define HYP_PAGE_OFFSET_LOW_MASK        ((UL(1) << (VA_BITS - 1)) - 1)
#define __hyp_text __section(.hyp.text) notrace

static inline unsigned long __kern_hyp_va(unsigned long v)
{
        asm volatile(ALTERNATIVE("and %0, %0, %1",
                                 "nop",
                                 ARM64_HAS_VIRT_HOST_EXTN)
                     : "+r" (v)
                     : "i" (HYP_PAGE_OFFSET_HIGH_MASK));
        asm volatile(ALTERNATIVE("nop",
                                 "and %0, %0, %1",
                                 ARM64_HYP_OFFSET_LOW)
                     : "+r" (v)
                     : "i" (HYP_PAGE_OFFSET_LOW_MASK));
        return v;
}

#define kern_hyp_va(v)  ((typeof(v))(__kern_hyp_va((unsigned long)(v))))

asm(	".align 11;\n");


int __hyp_text truly_test(struct truly_vm* cxt) 
{
	return 9999;
}

int func2(void);

int truly_test_code_map(void* cxt) 
{
	printk("%s\n",__FUNCTION__);
	return 2244;
}

EXPORT_SYMBOL_GPL(truly_test_code_map);

struct truly_vm  __percpu *tpvm;
EXPORT_SYMBOL_GPL(tpvm);

#define KERNEL_START _text
#define KERNEL_END	_end
/*
	kernel_code.start   = virt_to_phys(_text);
	kernel_code.end     = virt_to_phys(_etext - 1);
	kernel_data.start   = virt_to_phys(_sdata);
	kernel_data.end     = virt_to_phys(_end - 1);
*/

int truly_init(void)
{
	tp_info("Memory Layout code start=%p,%p\n "
			"code end =%p,%p\n"
			" sizeofcode=%d\n"
			"data start = %p,%p\n"
			" end = %p,%p,\n" "sizeofdata=%d\n",
			(void *)_text, (void*)virt_to_phys(_text),
			(void *)_end, (void *)virt_to_phys(_etext),
			(int)(virt_to_phys(_end) - virt_to_phys(_text)),
			(void*)_sdata, (void *)virt_to_phys(_sdata),
			(void*)_end, (void *)virt_to_phys(_end-1),
			(int)(virt_to_phys(_end-1) - virt_to_phys(_sdata) ) );

/*			 

	tp_host_state = alloc_percpu(tp_cpu_context_t);
	if (!tp_host_state) {
		tp_info("Cannot allocate host CPU state\n");
		return -1;
	}


	for_each_possible_cpu(cpu) {
	
		tp_cpu_context_t *cpuctxt;

		cpuctxt = per_cpu_ptr(tp_host_state, cpu);

		err = create_hyp_mappings(cpuctxt, cpuctxt + 1);

		if (err) {
			tp_info("Cannot map host CPU state: %d\n", err);
			return -1;
		}
	}

	err = create_hyp_mappings( _sdata,  _end -1);
	if (err){
		tp_info("Failed to global kernel data %d\n", err);
	}else{
		tp_info("Kernel data mapped SUCCESSFULLY\n");
	}

	err = create_hyp_mappings(_text, _etext);
	if (err){
		tp_info("Failed to map kernel code %d\n", err);
	}else{
		tp_info("Kernel Code mapped SUCCESSFULLY\n");
	}
*/
	tp_info("init sucessfully\n");
	return 0;
}


EXPORT_SYMBOL_GPL(truly_get_sctlr_el2);
EXPORT_SYMBOL_GPL(truly_get_tcr_el2);
EXPORT_SYMBOL_GPL(truly_set_tcr_el2);
EXPORT_SYMBOL_GPL(truly_set_mmu);

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

EXPORT_SYMBOL_GPL(truly_test_vec);


