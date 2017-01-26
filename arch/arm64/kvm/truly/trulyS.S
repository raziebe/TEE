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


.macro deactivate_traps
	mov     x2, #HCR_RW 
	orr	x2, x2, #HCR_SWIO
	msr	hcr_el2, x2
	msr	hstr_el2, xzr
	mrs	x2, mdcr_el2
	and	x2, x2, #MDCR_EL2_HPMN_MASK
	msr	mdcr_el2, x2
.endm

.macro activate_traps	
	mov	x2, #MDCR_EL2_TDA
	msr	mdcr_el2, x2
.endm

.align 11
ENTRY(__truly_vectors)
        ventry  EL2_sync                        // raz test
        ventry  EL2_irq_invalid                 // IRQ EL2t
        ventry  EL2_fiq_invalid                 // FIQ EL2t
        ventry  EL2_error_invalid               // Error EL2t

        ventry  EL2_sync  	              // Synchronous EL2h
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
	push	x0, x1
	push	x2, x3

	mrs	x1, esr_el2
	lsr	x2, x1, #ESR_ELx_EC_SHIFT	// Syndrom shift 26 bits

	cmp	x2, #ESR_ELx_EC_HVC64		// not 10110 then we have a trap
	b.ne	EL1_trap

#  vttbr  = virtualization translation table base register stage 2 
	mrs	x3, vttbr_el2			// If vttbr is valid, the 64bit guest, 
	cbnz	x3, EL1_trap			// called HVC, 

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

/*
*	here I need to check for various syndroms
*/
EL1_trap:
	mrs    x1, tpidr_el2  // get truly context 
	cbz    x1, 3f

//
//	store the exception code stored in x2
//	D.7-1933
//
	mrs	x3, esr_el1
        str     x3, [x1, #TP_CPU_OFFSET_DEBUG]
	cmp	x2,#0x18 // ISS encoded D7-1950
	b.eq	3f

        str     x2, [x1, #TP_CPU_OFFSET_ESR_EL2]

	// update  counter

        ldr     x3, [x1, #TP_CPU_OFFSET_COUNT]
        add     x3, x3, #1
        str     x3, [x1, #TP_CPU_OFFSET_COUNT]
//
//	Jump over the next address to return to el1
3:
	mrs x2, elr_el2
	add x2, x2 , #4
	msr elr_el2, x2
// exit out
4:
	pop	x2, x3
	pop	x0, x1
	eret

//	EL1 irq --> never saw this happens
EL1_irq:
	mov x6, #17
        b __truly_panic
	eret
ENDPROC(EL1_irq)


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
	" HPFAR:%016x RAZDBG:%016x\nTrulyCxt:%p\n\0"

.align 2

.macro invalid_vector   label,num
.align 2
\label:
	mov	x6, \num
        b __truly_panic
ENDPROC(\label)
.endm

ENTRY( EL2_sync )
	mov	x6, #323
        b __truly_panic
	eret
ENDPROC(EL2_sync)

invalid_vector  EL2_irq_invalid ,#2
invalid_vector  EL2_fiq_invalid, #3
invalid_vector  EL2_error_invalid, #4
invalid_vector  EL1_sync_invalid,#5
invalid_vector  EL1_irq_invalid,#6
invalid_vector  EL1_fiq_invalid,#7
invalid_vector  EL1_error_invalid,#8


ENTRY(truly_has_vhe)
        mrs     x2, id_aa64mmfr1_el1
        ubfx    x2, x2, #8, #4
	mov	x0,x2 // if zero then no vhe
ENDPROC(truly_has_vhe)

ENTRY(truly_set_hcr_el2)
	msr	 hcr_el2,x0
	ret
ENDPROC(truly_get_hcr_el2)

ENTRY(truly_get_hcr_el2)
	mrs	x0, hcr_el2
	ret
ENDPROC(truly_get_hcr_el2)

ENTRY(truly_set_mdcr_el2)
	msr	mdcr_el2,x0
	ret
ENDPROC(truly_set_mdcr_el2)

ENTRY(truly_get_mdcr_el2)
	mrs	x0, mdcr_el2
	ret
ENDPROC(truly_get_mdcr_el2)

ENTRY(truly_set_tpidr)
	kern_hyp_va	x0
	msr     tpidr_el2, x0   // Save truly context 
	ret
ENDPROC(truly_set_tpidr)

ENTRY(truly_get_tpidr)
	mrs     x0, tpidr_el2
	ret
ENDPROC(truly_get_tpidr)

// the virtualization translation base register for stage 2
ENTRY(truly_get_vttbr_el2)
	mrs	x0, vttbr_el2
	ret
ENDPROC(truly_get_vttbr_el2)

ENTRY(truly_get_ttbr0_el2)
	mrs	x0, ttbr0_el2
	ret
ENDPROC(truly_get_ttbr0_el2)

ENTRY(truly_get_ttbr1_el2)
	mrs	x0, ttbr1_el2
	ret
ENDPROC(truly_get_ttbr1_el2)

ENTRY(truly_set_ttbr1_el2)
	msr	ttbr1_el2,x0
	isb
	ret
ENDPROC(truly_set_ttbr1_el2)

ENTRY(truly_set_ttbr0_el2)
	msr	ttbr0_el2,x0
	ret
ENDPROC(truly_set_ttbr0_el2)

ENTRY(truly_get_tcr_el1)
	mrs	x0, tcr_el1
	isb
	ret
ENDPROC(truly_get_tcr_el1)

ENTRY(truly_get_tcr_el2)
	mrs	x0, tcr_el2
	ret
ENDPROC(truly_get_tcr_el2)

ENTRY(truly_set_tcr_el2)
	msr	tcr_el2,x0
	ret
ENDPROC(truly_set_tcr_el2)

ENTRY(truly_get_sctlr_el1)
	mrs	x0, sctlr_el1
	ret
ENDPROC(truly_get_sctlr_el1)

ENTRY(truly_get_sctlr_el2)
	mrs	x0, sctlr_el2
	ret
ENDPROC(truly_get_sctlr_el2)

ENTRY(truly_set_sctlr_el2)
	msr 	sctlr_el2, x0 
	isb
	ret
ENDPROC(truly_set_sctlr_el2)

ENTRY(truly_exec_el1)
	
	push	lr, xzr

	mov	lr, x0
	mov	x0, x1
	
	mrs	x3,contextidr_el1
	msr	contextidr_el2,x3

	mrs	x3,ttbr0_el1
	msr	ttbr0_el2,x3

	mrs	x3, tcr_el1
	msr	tcr_el2, x3

	mrs	x3, ttbr1_el1
	msr	ttbr1_el2, x3
	isb
	
#	mov	x3, #0
	orr	x3, x3, #(1 << 34) //  E2H = 1
	orr	x3, x3, #(1 << 27) // TGE = 1
	msr	hcr_el2, x3
	isb

//	turn on mmu
	mrs	x3, sctlr_el2
	orr	x3, x3, #(1<<0)	// mmu enable
	msr	sctlr_el2, x3
	isb

// Prepare to launch
	blr	lr
	pop	lr, xzr

// turn off the mmu
	mrs	x3, sctlr_el2
	eor	x3, x3, #1
	msr	sctlr_el2, x3
	isb

// turn 
	mrs	x3, hcr_el2
	eor	x3, x3, #(1 << 34) //  E2H = 1
	eor	x3, x3, #(1 << 27) // TGE = 1
	msr	hcr_el2, x3
	isb

	ret
ENDPROC(truly_exec_el1)

.popsection
