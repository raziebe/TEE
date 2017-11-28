#include <linux/module.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/sections.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <linux/vmalloc.h>


#define __TRULY_DEBUG__
#include <linux/truly.h>
#include <linux/tp_mmu.h>

DEFINE_PER_CPU(struct truly_vm, TVM);


phys_addr_t at_el0(unsigned long addr)
{
	phys_addr_t ret_addr;

    asm ("mov x0, %0\n"
        "at S1E0R,x0\n"
        "mrs x0, par_el1"
        		:"=r" (ret_addr)
        		: "r" (addr));

   return ret_addr;
}


struct truly_vm *get_tvm(void)
{
	struct truly_vm *tvm;

	preempt_disable();
	tvm = this_cpu_ptr(&TVM);
	preempt_enable();
	return tvm;
}

struct truly_vm *get_tvm_per_cpu(int cpu)
{
	return &per_cpu(TVM, cpu);
}

long truly_get_elr_el1(void)
{
	long e;

      asm("mrs  %0, elr_el1\n":"=r"(e));
	return e;
}

void truly_set_sp_el1(long e)
{
      asm("msr  sp_el1,%0\n":"=r"(e));
}

void truly_set_elr_el1(long e)
{
      asm("msr  elr_el1,%0\n":"=r"(e));
}

long truly_get_mfr(void)
{
	long e = 0;
    asm("mrs %0,id_aa64mmfr0_el1\n":"=r"(e));
	return e;
}

/*
 *the page us using attr_ind 4
 */
void make_mair_el2(struct truly_vm *tvm)
{
	unsigned long mair_el2;

	mair_el2 = tp_call_hyp(read_mair_el2);
	tvm->mair_el2 = (mair_el2 & 0x000000FF00000000L ) | 0x000000FF00000000L; //
}

void make_hstr_el2(struct truly_vm *tvm)
{
	tvm->hstr_el2 = 0;	// 1 << 15 ; // Trap CP15 Cr=15
}

void make_hcr_el2(struct truly_vm *tvm)
{
	tvm->hcr_el2 = HCR_TRULY_FLAGS;
}

void make_mdcr_el2(struct truly_vm *tvm)
{
	tvm->mdcr_el2 = 0x00;
}
/*
#define SCTLR_EL2_I_BIT_SHIFT		12
#define SCTLR_EL2_C_BIT_SHIFT		2
*/

void make_sctlr_el2(struct truly_vm *tvm)
{
	unsigned long sctlr_el2;

	// sctlr_el2 is been programmed by the initial vector.

	sctlr_el2 = tp_call_hyp(read_sctlr_el2);
	sctlr_el2 &= (~( (1 << SCTLR_EL2_A_BIT_SHIFT) | (1 << SCTLR_EL2_SA_BIT_SHIFT) ));
	tvm->sctlr_el2 = sctlr_el2;
}

static struct proc_dir_entry *procfs = NULL;

static ssize_t proc_write(struct file *file, const char __user * buffer,
			  size_t count, loff_t * dummy)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		struct truly_vm *tv = &per_cpu(TVM, cpu);
		tv->brk_count_el2 = 0;
		tv->copy_time = 0;
		tv->decrypt_time = 0;
	}

	return count;
}

static int proc_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *) 0x1;
	return 0;
}

static ssize_t proc_read(struct file *filp, char __user * page,
			 size_t size, loff_t * off)
{
	ssize_t len = 0;
	int cpu;

	if (filp->private_data == 0x00)
		return 0;

	for_each_possible_cpu(cpu) {
		struct truly_vm *tv = &per_cpu(TVM, cpu);
		len += sprintf(page + len, "cpu %d brk count %ld"
					" decrypt time=%ld "
					 "pad time =%ld"
					 "copy_time = %ld \n",
					cpu,
			       tv->brk_count_el2,
				   tv->decrypt_time,
				   tv->pad_time,
				   tv->copy_time);
	}

	filp->private_data = 0x00;
	return len;
}



static struct file_operations proc_ops = {
	.open = proc_open,
	.read = proc_read,
	.write = proc_write,
};


static void init_procfs(void)
{
	procfs =
	    proc_create_data("truly_stats", O_RDWR, NULL, &proc_ops, NULL);
}

/*
 * construct page table
*/
int truly_init(void)
{
	int err;
	long long tcr_el1;
	int t0sz;
	int t1sz;
	int ips;
	int pa_range;
	long id_aa64mmfr0_el1;
	struct truly_vm *_tvm;
	int cpu = 0;

	id_aa64mmfr0_el1 = truly_get_mfr();
	tcr_el1 = truly_get_tcr_el1();

	t0sz = tcr_el1 & 0b111111;
	t1sz = (tcr_el1 >> 16) & 0b111111;
	ips = (tcr_el1 >> 32) & 0b111;
	pa_range = id_aa64mmfr0_el1 & 0b1111;

	_tvm = get_tvm();
	memset(_tvm, 0x00, sizeof(*_tvm));
	tp_create_pg_tbl(_tvm);
	make_vtcr_el2(_tvm);
	make_hcr_el2(_tvm);
	make_mdcr_el2(_tvm);


	_tvm->enc = kmalloc(sizeof(struct encrypt_tvm), GFP_ATOMIC);
	encryptInit(_tvm->enc);

	err = create_hyp_mappings((char *)_tvm->enc,
				((char *) _tvm->enc) + sizeof(struct encrypt_tvm));
	if (err) {
		tp_err("Failed to map encrypted\n");
	} else {
		tp_info("Mapped encrypted\n");
	}

	for_each_possible_cpu(cpu) {
		struct truly_vm *tv = &per_cpu(TVM, cpu);
		if (tv != _tvm) {
			memcpy(tv, _tvm, sizeof(*_tvm));
		}
		INIT_LIST_HEAD(&tv->hyp_addr_lst);
		mutex_init(&tv->sync);
	}

	printk("TrulyProtect Version rt-mappings rc-1.0-rc12 loaded\n");
/*
	tp_info("HYP_PAGE_OFFSET_SHIFT=%x "
			"HYP_PAGE_OFFSET_MASK=%lx "
			"HYP_PAGE_OFFSET=%lx "
			"PAGE_MASK=%lx"
			"PAGE_OFFSET=%lx \n",
			(long) HYP_PAGE_OFFSET_SHIFT,
			(long) HYP_PAGE_OFFSET_MASK,
			(long) HYP_PAGE_OFFSET,
			PAGE_MASK,
			PAGE_OFFSET);
*/
	init_procfs();
	return 0;
}



void truly_map_tvm(void *d)
{
	int err;
	struct truly_vm *tv = get_tvm();

	if (tv->tv_flags & TVM_PROCESS_INIT)
		return;

	err = create_hyp_mappings(tv, tv + 1);
	if (err) {
		tp_err("Failed to map tvm");
	} else {
		tp_info("Mapped tvm");
	}
	mb();
}

void tp_run_vm(void *x)
{
	struct truly_vm *tvm = get_tvm();
	unsigned long vbar_el2;
	unsigned long vbar_el2_current =
	    (unsigned long) (KERN_TO_HYP(__truly_vectors));

	truly_map_tvm(NULL);
	vbar_el2 = truly_get_vectors();
	if (vbar_el2 != vbar_el2_current) {
		tp_info("vbar_el2 should restore\n");
		truly_set_vectors(vbar_el2);
	}
//	make_mair_el2(tvm);
	make_sctlr_el2(tvm);
	tp_call_hyp(truly_run_vm, tvm);
	tp_info("RUN VM.....\n");

	printk("TrulyProtect hcr_el2=%lX"
				"sctlr_el2=%lx"
				" mair_el2=%lX vtcr_el2=%X"
				"  vttbr_el2=%lX sp_el2=%lx\n",
				tvm->hcr_el2,
				tvm->sctlr_el2,
				tvm->mair_el2,
				tvm->vtcr_el2,
				tvm->vttbr_el2, tvm->sp_el2 );

}


unsigned long __hyp_text truly_get_ttbr0_el1(void)
{
	long ttbr0_el1;

      asm("mrs %0,ttbr0_el1\n":"=r" (ttbr0_el1));
      return ttbr0_el1;
}

unsigned long  truly_get_exception_level(void)
{
	long el;
	asm ("mrs	%0, CurrentEl\n":"=r"(el));
	return el;
}

void set_mdcr_el2(void *dummy)
{
	struct truly_vm *tvm = get_tvm();
	tvm->mdcr_el2 = 0x100L;
	tp_call_hyp(truly_set_mdcr_el2);
}

void truly_set_trap(void)
{
	on_each_cpu(set_mdcr_el2, NULL, 0);
}

void reset_mdcr_el2(void *dummy)
{
	struct truly_vm *tvm ;

	tvm = get_tvm();

	tvm->mdcr_el2 = 0x00;
	tp_call_hyp(truly_set_mdcr_el2);
}

void truly_reset_trap(void)
{
	on_each_cpu(reset_mdcr_el2, NULL, 0);
}

int tp_is_active_protected(void)
{
	return  get_tvm()->protected_pgd == truly_get_ttbr0_el1();
}

int  __hyp_text  tp_hyp_memcpy(unsigned char *dst,unsigned char *src,int size)
{
	int i = 0;

	// check for an alignment
	//
	if (( (long)dst & 0b11) != (long)dst)
			goto bytes_copy;

	if (( (long)src & 0b11) != (long)src)
			goto bytes_copy;

	if (size >= sizeof(long))  {

		int sz8 = (size>>3) << 3;
		long *d = (long *)dst;
		long *s = (long *)src;

		for (i = 0; i < sz8; i += sizeof(long)) {
			*d = *s;
			d++;
			s++;
		}
	}

	if (i == size)
		return i;

bytes_copy:
	for (; i < size; i++) {
		dst[i] = src[i];
	}
	return i;
}

int __hyp_text  tp_hyp_memset(char *dst,char tag,int size)
{
	int i;
	for (i = 0; i < size; i++)
		dst[i] = tag;
	return i;
}



EXPORT_SYMBOL_GPL(truly_get_vectors);
EXPORT_SYMBOL_GPL(truly_get_hcr_el2);
EXPORT_SYMBOL_GPL(tp_call_hyp);
EXPORT_SYMBOL_GPL(truly_init);
