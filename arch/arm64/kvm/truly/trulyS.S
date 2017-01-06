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
.pushsection	.hyp.text, "ax" // borrow  hyp.text

.align 11


EL1_sync:
	push	x0, x1
	push	x2, x3

	mrs	x1, esr_el2
	lsr	x2, x1, #ESR_ELx_EC_SHIFT	// Syndrom shift 26 bits

	cmp	x2, #ESR_ELx_EC_HVC64		// not 10110 then we have a trap
	b.ne	EL1_trap

#	mrs	x3, vttbr_el2			// If vttbr is valid, the 64bit guest, 
//vttbr  = virtualization translation table base register 
#	cbnz	x3, el1_trap			// called HVC, --> does not happen

	/* Here, we're pretty sure the host called HVC. */
	pop	x2, x3
	pop	x0, x1

	/* Check for __hyp_get_vectors */
	cbnz	x0, 1f
	mrs	x0, vbar_el2
	b	2f

1:	push	lr, xzr

	/*
	 * Compute the function address in EL2, and shuffle the parameters.
	 */
	kern_hyp_va	x0
	mov	lr, x0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	blr	lr

	pop	lr, xzr
2:	eret

//
//	here I need to check for various syndroms
//
EL1_trap:
  //      mov x23, #888
        eret

//
//	EL1 irq --> never saw this happens
EL1_irq:
	ret
ENDPROC(EL1_irq)


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

ENTRY(truly_get_hcr_el2)
	mrs	x0, hcr_el2
	ret
ENDPROC(truly_get_hcr_el2)

ENTRY(truly_set_mdcr_el2)
	msr	mdcr_el2,x0
	ret
ENDPROC(truly_set_mdcr_el2)

//
// still the vbar from kvm. But use kvm allocated stack.
//  Still needs to set the vttbr_el2
ENTRY(truly_set_vbar)
       msr	vbar_el2, x0
       mov	x0,#432	// be sure
       ret
ENDPROC(truly_set_vbar)


.popsection
