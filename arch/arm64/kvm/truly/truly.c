#include <linux/compiler.h>
#include <linux/kvm_host.h>
#include <asm/kvm_mmu.h> 

#include <linux/module.h>
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


int __hyp_text truly_test(tp_cpu_context_t* cxt) 
{
	return 9999;
}

int __kvm_test_active_vm(void); EXPORT_SYMBOL_GPL(__kvm_test_active_vm);


unsigned long truly_get_vttbr_el2(void);
EXPORT_SYMBOL_GPL(truly_get_vttbr_el2);

// debug
unsigned long truly_get_mdcr_el2(void);
EXPORT_SYMBOL_GPL(truly_get_mdcr_el2);

int truly_set_mdcr_el2(int mask);
EXPORT_SYMBOL_GPL(truly_set_mdcr_el2);

int truly_test_mem(unsigned long);
EXPORT_SYMBOL_GPL(truly_test_mem);

void truly_set_tpidr(unsigned long tpidr);
EXPORT_SYMBOL_GPL(truly_set_tpidr);

unsigned long truly_get_tpidr(void);
EXPORT_SYMBOL_GPL(truly_get_tpidr);

extern int truly_test_vec(void);
EXPORT_SYMBOL_GPL(truly_test_vec);

unsigned long truly_get_hcr_el2(void);
EXPORT_SYMBOL_GPL(truly_get_hcr_el2);

void truly_set_hcr_el2(unsigned long tpidr);
EXPORT_SYMBOL_GPL(truly_set_hcr_el2);


tp_cpu_context_t __percpu *tp_host_state;
EXPORT_SYMBOL_GPL(tp_host_state);

int truly_init(void)
{
	int cpu;

	tp_info("init start  sizeof(tp_cpu_context_t) %ld \n", 
			sizeof(tp_cpu_context_t) );

	tp_host_state = alloc_percpu(tp_cpu_context_t);
	if (!tp_host_state) {
		tp_info("Cannot allocate host CPU state\n");
		return -1;
	}

	for_each_possible_cpu(cpu) {
		int err;
	
		tp_cpu_context_t *cpuctxt;

		cpuctxt = per_cpu_ptr(tp_host_state, cpu);

		err = create_hyp_mappings(cpuctxt, cpuctxt + 1);

		if (err) {
			tp_info("Cannot map host CPU state: %d\n", err);
			return -1;
		}
	}

	tp_info("init sucessfully\n");
	return 0;
}
