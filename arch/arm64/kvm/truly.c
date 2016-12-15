#include <asm/sections.h>

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kvm_host.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <asm/cputype.h>
#include <asm/uaccess.h>
#include <asm/kvm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_coproc.h>

// .pushsection    .hyp.text, "ax"
asm(".pushsection .hyp.text, \"ax\" \n\t" );

int truly_enter(void)
{
	return 888;
} 

asm(".popsection\n\t");

EXPORT_SYMBOL_GPL(truly_enter);
