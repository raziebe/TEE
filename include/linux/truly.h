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


unsigned long truly_test_code_map(void* vm);
unsigned long truly_get_sctlr_el2(void);
unsigned long truly_get_sctlr_el1(void);
unsigned long truly_get_tcr_el2(void);
unsigned long truly_get_tcr_el1(void);
unsigned long truly_get_ttbr0_el2(void);
unsigned long truly_get_ttbr1_el2(void);

void truly_set_sctlr_el2(unsigned long);
void truly_set_ttbr1_el2(unsigned long t);
void truly_set_ttbr0_el2(unsigned long t);
void truly_set_tcr_el2(unsigned long);
void truly_exec_el1(void *vm);
int truly_test_vttbr(void *cxt);
unsigned long truly_has_vhe(void);
unsigned long truly_get_hcr_el2(void);
unsigned long truly_get_mdcr_el2(void);
unsigned long truly_get_tpidr(void);
unsigned long truly_get_vttbr_el2(void);

unsigned long truly_test_vec(void);

void truly_set_hcr_el2(unsigned long);
void truly_set_mdcr_el2(unsigned long);
void truly_set_tpidr(unsigned long);

int truly_test(struct truly_vm* cxt);
long truly_get_mem_regs(void *cxt);

#define tp_info(fmt, ...) \
	pr_info("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#define tp_err(fmt, ...) \
	pr_err("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#endif
