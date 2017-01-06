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
.pushsection	.hyp.text, "axw" // borrow  hyp.text

ENTRY(__truly_vectors)
        ventry  EL2_sync_invalid                        // raz test
        ventry  EL2_irq_invalid                 // IRQ EL2t
        ventry  EL2_fiq_invalid                 // FIQ EL2t
        ventry  EL2_error_invalid               // Error EL2t

        ventry  EL2_sync_invalid                // Synchronous EL2h
        ventry  EL2_irq_invalid                 // IRQ EL2h
        ventry  EL2_fiq_invalid                 // FIQ EL2h
        ventry  EL2_error_invalid               // Error EL2h

        ventry  EL1_sync                        // Synchronous 64-bit EL1

        ventry  EL1_irq                       // IRQ 64-bit EL1
        ventry  EL1_fiq_invalid                 // FIQ 64-bit EL1
        ventry  EL1_error_invalid               // Error 64-bit EL1

        ventry  EL1_irq                // Synchronous 32-bit EL1
        ventry  EL1_irq_invalid                 // IRQ 32-bit EL1
        ventry  EL1_fiq_invalid                 // FIQ 32-bit EL1
        ventry  EL1_error_invalid               // Error 32-bit EL1
ENDPROC(__truly_vectors)

EL1_sync:
        mov x23, #888
        eret
ENDPROC(EL1_sync)


EL1_irq:
        mov x23, #888
        eret
ENDPROC(EL1_irq)

.align 11

.macro invalid_vector   label
\label:
        b \label
ENDPROC(\label)
.endm

        invalid_vector  EL2_sync_invalid
        invalid_vector  EL2_irq_invalid
        invalid_vector  EL2_fiq_invalid
        invalid_vector  EL2_error_invalid
        invalid_vector  EL1_sync_invalid
        invalid_vector  EL1_irq_invalid
        invalid_vector  EL1_fiq_invalid
        invalid_vector  EL1_error_invalid
.align 11


ENTRY( truly_enter_S )
       stp     x29, x30, [sp,#-16]!
       mov     x29, sp
 
//     call the function
       kern_hyp_va     x0
       blr x0
// back
       ldp    	 x29, x30, [sp],#16
       ret
ENDPROC(truly_enter_S)

ENTRY( truly_irq_S )
       ret
ENDPROC(truly_irq_S)

// we stll the vbar from kvm. we use kvm allocated stack.
ENTRY(truly_set_vbar)
       msr	vbar_el2,x0
       mov	x0,#432	// be sure
ENDPROC(truly_set_vbar)


.popsection
