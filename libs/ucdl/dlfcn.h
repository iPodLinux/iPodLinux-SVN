#ifndef _DLFCN_H_
#define _DLFCN_H_

#ifdef __cplusplus
extern "C" {
#endif

void *dlopen(const char *filename, int flag);
char *dlerror(void);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);

#define RTLD_LAZY     1
#define RTLD_NOW      2
#define RTLD_GLOBAL   3
#define RTLD_LOCAL    4
#define RTLD_NODELETE 5
#define RTLD_NOLOAD   6
#define RTLD_DEEPBIND 7

#ifdef __cplusplus
}
#endif

#endif
