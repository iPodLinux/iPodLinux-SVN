#include <pz.h>

void pthread_create() 
{
    pz_error ("pthread_create() attempt");
}

void pthread_getspecific()
{
    pz_error ("pthread_getspecific() attempt");
}

void pthread_key_create()
{
    pz_error ("pthread_key_create() attempt");
}

void pthread_mutex_lock()
{
    pz_error ("pthread_mutex_lock() attempt");
}

void pthread_mutex_unlock()
{
    pz_error ("pthread_mutex_unlock() attempt");
}

void pthread_once()
{
    pz_error ("pthread_once() attempt");
}

void pthread_setspecific()
{
    pz_error ("pthread_setspecific() attempt");
}

void no_init() {}
PZ_MOD_INIT (no_init)
