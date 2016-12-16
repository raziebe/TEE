#include <linux/linkage.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/assembler.h>
#include <asm/memory.h>
#include <asm/asm-offsets.h>
#include <asm/kvm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>

.text
.pushsection	.hyp.text, "ax"

/*
 * Save the VGIC CPU state into memory
 * x0: Register pointing to VCPU struct
 * Do not corrupt x1!!!
 */
ENTRY( truly_enter_S )
       stp     x29, x30, [sp,#-16]!
       mov     x29, sp
 
//     call the function
       ldr	x0,=truly_enter
       kern_hyp_va     x0
       blr x0
// back
       ldp    	 x29, x30, [sp],#16
       ret
ENDPROC(truly_enter_S)

.popsection
