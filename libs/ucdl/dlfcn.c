#include "ucdl.h"

void *dlopen(const char *filename, int flag)
{
	return uCdl_open(filename);
}
char *dlerror()
{
	return (char *)uCdl_error();
}
void *dlsym(void *handle, const char *symbol)
{
	return uCdl_sym(handle, symbol);
}
int dlclose(void *handle)
{
	uCdl_close(handle);
	return 0;
}
