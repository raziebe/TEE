
//asm( 	".pushsection .hyp.text, \"ax\" \n;" 


asm(	".align 11;\n");

int truly_enter(void) 
{
	return 0x888;
} 

int truly_enter(void) __attribute__ (( section (".hyp.text") ));
