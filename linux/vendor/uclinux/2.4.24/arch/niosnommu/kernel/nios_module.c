#include <linux/module.h>
#include <asm/module.h>
#include <asm/nios.h>

unsigned long arch_init_modules(struct module *mod)
{
	struct module_symbol *msym;
	unsigned int j;
	
	/* fix the init and cleanup routine*/
	if (mod->init)
		((unsigned long)mod->init)>>=1;
	if (mod->cleanup)
		((unsigned long)mod->cleanup)>>=1;

	for (j = 0, msym = mod->syms; j < mod->nsyms; ++j, ++msym) {
		/* no better way we can find to determine if it is a func */
		if (msym && (msym->value < (unsigned int)nasys_program_mem))
			msym->value <<=1;
	}
	
	return 0;	
}
