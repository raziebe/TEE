#include <asm/sections.h>
#include <linux/module.h>

// .pushsection    .hyp.text, "ax"
asm(".pushsection .hyp.text, \"ax\" \n\t" );

int truly_enter(void)
{
	return 888;
} 

asm(".popsection\n\t");

int truly_enter_S(void);
EXPORT_SYMBOL_GPL(truly_enter_S);
EXPORT_SYMBOL_GPL(truly_enter);
