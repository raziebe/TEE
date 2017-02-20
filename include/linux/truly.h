#ifndef _TRULY_H_
#define _TRULY_H_

// page 1775
#define DESC_TABLE_BIT 			( UL(1) << 1 )
#define DESC_VALID_BIT 			( UL(1) << 0 )
#define DESC_XN	       			( UL(1) << 54 )
#define DESC_CONTG_BIT		 	( UL(1) << 52 )
#define DESC_AF	        		( UL(1) << 10 )
#define DESC_SHREABILITY_SHIFT		(8)
#define DESC_S2AP_SHIFT			(6)
#define DESC_MEMATTR_SHIFT		(2)

#define VTCR_EL2_T0SZ_BIT_SHIFT 	0
#define VTCR_EL2_SL0_BIT_SHIFT 		6
#define VTCR_EL2_IRGN0_BIT_SHIFT 	8
#define VTCR_EL2_ORGN0_BIT_SHIFT 	10
#define VTCR_EL2_SH0_BIT_SHIFT 		12
#define VTCR_EL2_TG0_BIT_SHIFT 		14
#define VTCR_EL2_PS_BIT_SHIFT 		16

#define SCTLR_EL2_EE_BIT_SHIFT		25
#define SCTLR_EL2_WXN_BIT_SHIFT		19
#define SCTLR_EL2_I_BIT_SHIFT		12
#define SCTLR_EL2_C_BIT_SHIFT		2
#define SCTLR_EL2_A_BIT_SHIFT		1	
#define SCTLR_EL2_M_BIT_SHIFT		0

/* Hyp Configuration Register (HCR) bits */
#define HCR_ID		(UL(1) << 33)
#define HCR_CD		(UL(1) << 32)
#define HCR_RW_SHIFT	31
#define HCR_RW		(UL(1) << HCR_RW_SHIFT)
#define HCR_TRVM	(UL(1) << 30)
#define HCR_HCD		(UL(1) << 29)
#define HCR_TDZ		(UL(1) << 28)
#define HCR_TGE		(UL(1) << 27)
#define HCR_TVM		(UL(1) << 26)
#define HCR_TTLB	(UL(1) << 25)
#define HCR_TPU		(UL(1) << 24)
#define HCR_TPC		(UL(1) << 23)
#define HCR_TSW		(UL(1) << 22)
#define HCR_TAC		(UL(1) << 21)
#define HCR_TIDCP	(UL(1) << 20)
#define HCR_TSC		(UL(1) << 19)
#define HCR_TID3	(UL(1) << 18)
#define HCR_TID2	(UL(1) << 17)
#define HCR_TID1	(UL(1) << 16)
#define HCR_TID0	(UL(1) << 15)
#define HCR_TWE		(UL(1) << 14)
#define HCR_TWI		(UL(1) << 13)
#define HCR_DC		(UL(1) << 12)
#define HCR_BSU		(3 << 10)
#define HCR_BSU_IS	(UL(1) << 10)
#define HCR_FB		(UL(1) << 9)
#define HCR_VA		(UL(1) << 8)
#define HCR_VI		(UL(1) << 7)
#define HCR_VF		(UL(1) << 6)
#define HCR_AMO		(UL(1) << 5)
#define HCR_IMO		(UL(1) << 4)
#define HCR_FMO		(UL(1) << 3)
#define HCR_PTW		(UL(1) << 2)
#define HCR_SWIO	(UL(1) << 1)
#define HCR_VM		(UL(1) << 0)

#define HCR_GUEST_FLAGS (HCR_TSC | HCR_TSW | HCR_TWE | HCR_TWI | HCR_VM | \
			 HCR_TVM | HCR_BSU_IS | HCR_FB | HCR_TAC | \
			 HCR_AMO | HCR_SWIO | HCR_TIDCP | HCR_RW)

struct truly_vm {

	unsigned long count;
	unsigned long debug;
    unsigned long hcr_el2;
 	unsigned long hstr_el2;
 	unsigned long ttbr0_el2;
 	unsigned long ttbr1_el2;
 	unsigned long vttbr_el2;
 	unsigned long vtcr_el2;

 	unsigned long tpidr_el2;
 	unsigned long tcr_el2;
 	unsigned long sp_el2;
    unsigned long elr_el2;
  	unsigned long esr_el2;
    unsigned long sp_el0;
 	unsigned long spsr_el2;
  	
    unsigned long vbar_el2;
 	unsigned long sctlr_el2;
 	void* pg_lvl_one;

 	unsigned long tcr_el1;
 	unsigned long sp_el1;
 	unsigned long elr_el1;
 	unsigned long spsr_el1;
 	unsigned long ttbr0_el1;
 	unsigned long ttbr1_el1;
 	unsigned long vttbr_el1;
 	unsigned long vbar_el1;
 	unsigned long sctlr_el1;

	unsigned long id_aa64mmfr0_el1;
};


unsigned long truly_get_sctlr_el2(void);
unsigned long truly_get_sctlr_el1(void);
unsigned long truly_get_tcr_el2(void);
unsigned long truly_get_tcr_el1(void);
unsigned long truly_get_ttbr0_el2(void);
unsigned long truly_get_ttbr1_el2(void);

void truly_set_vttbr_el2(long vttbr_el2);
void truly_set_sctlr_el2(unsigned long);
void truly_set_ttbr1_el2(unsigned long t);
void truly_set_ttbr0_el2(unsigned long t);
void truly_set_tcr_el2(unsigned long);
void truly_exec_el1(void *vm);
void truly_exec_el1_2(void *vm);
int truly_test_vttbr(void *cxt);
unsigned long truly_has_vhe(void);
unsigned long truly_get_hcr_el2(void);
unsigned long truly_get_mdcr_el2(void);
unsigned long truly_get_tpidr(void);
unsigned long truly_get_vttbr_el2(void);

struct truly_vm* get_tvm(void);

void truly_set_hcr_el2(unsigned long);
void truly_set_mdcr_el2(unsigned long);
void truly_set_tpidr(unsigned long);

int truly_run_vm(struct truly_vm* cxt);
long truly_get_mem_regs(void *cxt);

#define tp_info(fmt, ...) \
	pr_info("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#define tp_err(fmt, ...) \
	pr_err("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#endif
