#include <linux/compiler.h>
#include <linux/kvm_host.h>
#include <asm/kvm_mmu.h> 

#include <linux/module.h>

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

int xx = 3;

int __hyp_text foo2(void)
{
	return 222;
}

static   __hyp_text int foo3(void)
{
	return 333;
}

static __hyp_text int foo1(int x)
{
	if (x == 1)
		return kern_hyp_va(foo2());
	return kern_hyp_va(foo3());
}

int __hyp_text truly_enter(int _x) 
{
	return  kern_hyp_va(foo1(_x));

} 
