
//asm( 	".pushsection .hyp.text, \"ax\" \n;" 
#include <linux/module.h>

asm(	".align 11;\n");

int x;

static inline int foo2(void)
{
	return 222;
}

static inline int foo3(void)
{
	return 333;
}

static inline int foo1(void)
{
	if (x == 1)
		return foo2();
	return foo3();

}

int truly_enter(void) 
{
	x = 2;
	return foo1();
} 

int truly_enter(void) __attribute__ (( section (".hyp.text") ));
