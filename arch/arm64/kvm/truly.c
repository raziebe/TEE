
//asm( 	".pushsection .hyp.text, \"ax\" \n;" 


asm(	".align 11;\n");

int truly_enter(void) __attribute__ (( section (".hyp.text.\"ax\"") ));

int truly_enter(void) 
{
	return 888;
} 

