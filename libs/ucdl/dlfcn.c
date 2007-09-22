#include "ucdl.h"
static int init = 0;

#define SELF "/proc/self/exe"

void *dlopen(const char *filename, int flag)
{
	if (!init) init = uCdl_init(SELF);
	return uCdl_open(filename);
}
char *dlerror()
{
	if (!init) init = uCdl_init(SELF);
	return (char *)uCdl_error();
}
void *dlsym(void *handle, const char *symbol)
{
	if (!init) init = uCdl_init(SELF);
	return uCdl_sym(handle, symbol);
}
int dlclose(void *handle)
{
	if (!init) init = uCdl_init(SELF);
	uCdl_close(handle);
	return 0;
}
