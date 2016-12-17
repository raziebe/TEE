
//asm( 	".pushsection .hyp.text, \"ax\" \n;" 
#include <linux/module.h>

asm(	".align 11;\n");

int truly_enter(void) 
{
	printk("XXXXXXXXXXX\n");
	return 0x888;
} 

int truly_enter(void) __attribute__ (( section (".hyp.text") ));
