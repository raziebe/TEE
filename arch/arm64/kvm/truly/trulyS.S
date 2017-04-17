#include <linux/linkage.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/assembler.h>
#include <asm/memory.h>
#include <asm/asm-offsets.h>
#include <asm/kvm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_mmu.h>
#include <asm/alternative.h>
#include <asm/cpufeature.h>

.text
.pushsection	.hyp.text, "ax" // borrow  hyp.text
.align	PAGE_SHIFT

.macro invalid_vector   label,num
.align 7
\label:
	mov	x6, \num
        b __truly_panic
ENDPROC(\label)
.endm


.macro save_registers
	push x0,x1
	push x2,x3
	push x4,x5
	push x6,x7
	push x8,x9
	push x10,x11
	push x12,x13
	push x14,x15
	push x16,x17
	push x18,x19
	push x20,x21
	push x22,x23
	push x24,x25
	push x26,x27
	push x28,x29
	push x30,xzr
.endm


.macro restore_registers
	pop x30,xzr
	pop x28,x29
	pop x26,x27
	pop x24,x25
	pop x22,x23
	pop x20,x21
	pop x18,x19
	pop x16,x17
	pop x14,x15
	pop x12,x13
	pop x10,x11
	pop x8,x9
	pop x6,x7
	pop x4,x5
	pop x2,x3
	pop x0,x1
.endm

//  code taken from kvm

EL1_sync:
	push	x0, x1
	push	x2, x3

	mrs	x1, esr_el2
	lsr	x2, x1, #ESR_ELx_EC_SHIFT	// Syndrom shift 26 bits

	cmp	x2, #ESR_ELx_EC_HVC64		// If not 10110 then we have a trap
	b.ne	el1_trap


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
	mov	lr, x0		// function address
	mov	x0, x1		// the context
	mov	x1, x2
	mov	x2, x3
	blr	lr

	pop	lr, xzr
	
2:	eret

el1_trap:
	/*
	 * x1: ESR
	 * x2: ESR_EC
	 */

	/* Guest accessed VFP/SIMD registers, save host, restore Guest */
	cmp	x2, #ESR_ELx_EC_FP_ASIMD		// 0x7
	b.eq	4f
	
	cmp x2, #0x18				// MRC,MRS
	b.eq	4f
	cmp x2, #0x3c					// debug brk command
	b.eq	brk_exception
	
	cmp	x2, #ESR_ELx_EC_DABT_LOW	// ESR_ELx_EC_DABT_LOW = 0x24 = 36d = data abort
	mov	x0, #ESR_ELx_EC_IABT_LOW	// ESR_ELx_EC_IABT_LOW = 0x20 = 32 instruction abort 
	ccmp	x2, x0, #4, ne // if and 4 && (x2 and x0 ) then br to 1
	b.ne	4f		// Not an abort we care about


4:
	pop	x2, x3
	pop	x0, x1
	
	eret
 

brk_exception:


	mrs	x0,tpidr_el2
	mrs  x2,elr_el2
	str x2,[x0, #TP_ELR_EL2]
	str x30,[x0, #TP_X30]	// save the caller address

	save_registers

	ldr x1,=truly_decrypt
	kern_hyp_va x1

	blr x1

	mrs  x2,elr_el2
	ic ialluis

	restore_registers
	pop	x2, x3
	pop	x0, x1

	msr cntvoff_el2 ,x1   //<-- save x1

//	force stack to el0
	mrs x1, spsel
	and w1,w1,#0xFFFFFFFE
	msr spsel,x1

	mrs x30,elr_el2	   // go back to the next command after the faulted one
	mrs x1,cntvoff_el2 // <-- restore x1. Must return with the exact same registers

	blr x30			// Execute decrypted in EL2  and then return to next command because of the blr call.
	// User called ret.
	// Return from the decrypted routine and then return to the
	// reconstruced x30.
	// We use a user's stack
	push x0, x1
	mrs	x0,tpidr_el2
	// reconstrcted caller
	ldr x30,[x0, #TP_X30]
	str xzr,[x0, #TP_X30]
	pop x0,x1

	msr cntvoff_el2 ,x1   //<-- save x1
	//	force stack back to elX
	mrs x1, spsel
	orr w1,w1,#0x1
	msr spsel,x1
	mrs x1,cntvoff_el2 // <-- restore x1

	eret
ENDPROC(el1_trap)



__truly_panic:
	adr	x0, __hyp_panic_str
	adr	x1, 2f // adr generates a register-relative address in the destination register
	ldp	x2, x3, [x1] // Load to Pair of Registers from two dwords starting from memory at [x1] 
	sub	x0, x0, x2
	add	x0, x0, x3
	mrs	x1, spsr_el2
	mrs	x2, elr_el2
	mrs	x3, esr_el2
	mrs	x4, far_el2
	mrs	x5, hpfar_el2
	mrs	x7, tpidr_el2

	mov	lr, #(PSR_F_BIT | PSR_I_BIT | PSR_A_BIT | PSR_D_BIT |\
		      PSR_MODE_EL1h)
	msr	spsr_el2, lr
	ldr	lr, =panic
	msr	elr_el2, lr
	eret

	.align	3
2:	.quad	HYP_PAGE_OFFSET
	.quad	PAGE_OFFSET
ENDPROC(__truly_panic)
__hyp_panic_str:
	.ascii	"Truly panic:\nCode:%08x PC:%016x ESR:%08x\nFAR:%016x" \
	" HPFAR:%016x RAZDBG:%p\nTrulyCxt:%p\n\0"

ENTRY(truly_get_tcr_el1)
       mrs     x0, tcr_el1
       isb
       ret
ENDPROC(truly_get_tcr_el1)

// This procedure calls the default hypervisor vector and
// and sets truly vector. This is because when the cpu drops
// Linux calls smc and vbar_el2 reseets.
ENTRY(truly_get_vectors)
	mov	x0,xzr
ENTRY(truly_set_vectors)
	hvc #0
	ret
ENDPROC(truly_set_vectors)


ENTRY(truly_get_hcr_el2)
       mrs     x0, hcr_el2
       ret
ENDPROC(truly_get_hcr_el2)

/*
ENTRY(tp_get_ttbr0_el2)
       mrs     x0, ttbr0_el2
       ret
ENDPROC(tp_get_ttbr0_el2)
*/

ENTRY(tp_call_hyp)
        hvc	#0
        ret
ENDPROC(tp_call_hyp)


ENTRY(truly_has_vhe)
        mrs     x2, id_aa64mmfr1_el1
        ubfx    x2, x2, #8, #4
		mov	x0,x2 // if zero then no vhe
ENDPROC(truly_has_vhe)


ENTRY(truly_run_vm)
	
	pop	lr, xzr
	mov	x4, lr		// save the link register of EL1 before losing it.
	
	kern_hyp_va  x0	// grab tvm
	msr	tpidr_el2, x0	// save tvm context
	

	ldr	x1, [x0, #TP_HCR_EL2]
	kern_hyp_va x1
    msr     hcr_el2, x1
	
 	ldr x1, [x0, #TP_HSTR_EL2]
	kern_hyp_va x1
    msr hstr_el2, x1

    ldr		x1, [x0, #TP_VTCR_EL2]
	kern_hyp_va x1
	msr	vtcr_el2, x1
 
    ldr     x1, [x0, #TP_VTTBR_EL2]
    kern_hyp_va x1
    msr vttbr_el2, x1

    ldr		x1, [x0, #TP_MDCR_EL2]
	kern_hyp_va x1
	msr		mdcr_el2, x1
	// Clear cntvoff for the host
	msr	cntvoff_el2, xzr

/*	check here that we storge the keys in the registers.
			0x2b,0x7e,0x15,0x16,
			0x28,0xae,0xd2,0xa6,
			0xab,0xf7,0x15,0x88,
			0x09,0xcf,0x4f,0x3c};
	amair_el2  actlr_el2

*/
	mov		x1, #0
	mov     x1, #0xd2a6
    movk    x1, #0x28ae, lsl #16
    movk    x1, #0x1516, lsl #32
    movk    x1, #0x2b7e, lsl #48

	msr vmpidr_el2, x1

	mov		x1, #0
	mov     x1, #0xd2a6
    movk    x1, #0x28ae, lsl #16
    movk    x1, #0x1516, lsl #32
    movk    x1, #0x2b7e, lsl #48

	msr amair_el2, x1


	mov	lr,x4
	eret // must eret here. stack pointer is changed 
ENDPROC(truly_run_vm)


EL2_sync:

	save_registers
//
// Move to previous position
// Note, I must check if all faults here are the same
//
	mrs x1, elr_el2
	sub x1,x1,#4
	mrs	x0,tpidr_el2
	str x1,[x0, #TP_ELR_EL2]
	msr elr_el2, x1 // eret will start from here.

	ldr x1,=truly_pad
	kern_hyp_va x1
	blr x1

	ic ialluis	// flash the entire cache, TODO here.

	restore_registers

	msr cntvoff_el2 ,x1  //<-- save x1 for the mode switch

//	force stack back to elX
	mrs x1,spsel
	and w1,w1,#0xFFFFFFFE
	msr spsel,x1

// force switch to EL0
	mrs x1,spsr_el2
	and w1,w1,#0xFFFFFFC0
	msr spsr_el2,x1
	msr spsr_el1,x1

	mrs x1,cntvoff_el2 // <-- restore x1. We move to EL0

	eret
ENDPROC(EL2_sync)



// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.den0024a/CHDEEDDC.html
.align 11
ENTRY(__truly_vectors)
        ventry EL2_sync                        //Current EL with SP0
        invalid_vector  EL2_irq_invalid,#2                 // IRQ EL2t
        invalid_vector  EL2_fiq_invalid,#3                // FIQ EL2t
        invalid_vector  EL2_error_invalid,#4             // Error EL2t

        invalid_vector  EL2_syncELSpx,#5  	              // Current EL with SPx
        invalid_vector  EL2_irq_invalidELSpx,#6                 // IRQ EL2h
        invalid_vector  EL2_fiq_invalidELSpx,#7                 // FIQ EL2h
        invalid_vector  EL2_error_invalidELspx,#8               // Error EL2h

        ventry  EL1_sync                        			// Synchronous 64-bit EL1
        invalid_vector  EL1_irq,#14                       				// IRQ 64-bit EL1
        invalid_vector  EL1_fiq_invalid ,#9                // FIQ 64-bit EL1
        invalid_vector  EL1_error_invalid  ,#10             // Error 64-bit EL1

        ventry  EL1_irq  	              							// Synchronous 32-bit EL1
        invalid_vector  EL1_irq_invalid,#11                 		// IRQ 32-bit EL1
        invalid_vector  EL1_fiq_invalidLowEL32 ,#12               // FIQ 32-bit EL1
        invalid_vector  EL1_error_invalidLowEL32,#13               // Error 32-bit EL1
ENDPROC(__truly_vectors)

.popsection
