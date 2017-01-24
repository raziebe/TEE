#ifndef _TRULY_H_
#define _TRULY_H_

struct truly_vm {

	unsigned long count; 
	unsigned long debug;
       	unsigned long hcr_el2;
 	unsigned long hstr_el2;
 	unsigned long ttbr0_el2;
 	unsigned long ttbr1_el2;
 	unsigned long tpidr_el2;
 	unsigned long tcr_el2;
 	unsigned long sp_el2;
    	unsigned long elr_el2;
  	unsigned long esr_el2;
    	unsigned long sp_el0;
 	unsigned long spsr_el2;
  	unsigned long vttbr_el2;
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
};


unsigned long truly_test_code_map(void* cxt);
unsigned long truly_get_sctlr_el2(void);
unsigned long truly_get_sctlr_el1(void);
unsigned long truly_get_tcr_el2(void);
unsigned long truly_get_tcr_el1(void);
unsigned long truly_get_ttbr0_el2(void);
unsigned long truly_get_ttbr1_el2(void);

void truly_set_ttbr1_el2(unsigned long t);
void truly_set_ttbr0_el2(unsigned long t);
void truly_set_tcr_el2(unsigned long);
void truly_set_mmu(void);

unsigned long truly_has_vhe(void);
unsigned long truly_get_hcr_el2(void);
unsigned long truly_get_mdcr_el2(void);
unsigned long truly_get_tpidr(void);
unsigned long truly_get_vttbr_el2(void);

unsigned long truly_test_vec(void);

void truly_set_hcr_el2(unsigned long);
void truly_set_mdcr_el2(unsigned long);
void truly_set_tpidr(unsigned long);

#define tp_info(fmt, ...) \
	pr_info("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#define tp_err(fmt, ...) \
	pr_err("truly [%i]: " fmt, task_pid_nr(current), ## __VA_ARGS__)

#endif
