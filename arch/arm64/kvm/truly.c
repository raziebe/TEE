
asm(".pushsection .hyp.text, \"ax\" \n\t" );


static void _mcount(void){};

int truly_enter(void)
{
	return 888;
} 

asm(".popsection\n\t");
