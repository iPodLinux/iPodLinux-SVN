#include <stdio.h>
#include "ucdl.h"

int called = 0;

void callme (int param) 
{
    if (param != 0xdeadc0de) {
	fprintf (stderr, "F (%x)\n", param);
	exit (5);
    } else fprintf (stderr, "P");
    called++;
}

int main(int argc, char **argv) 
{
    void *handle;
    int (*modtest)() = 0;

    if (!uCdl_init (argv[0])) {
	fprintf (stderr, "F\n");
	exit (1);
    } else fprintf (stderr, "P");
    if (!(handle = uCdl_open ("testmod.o"))) {
	fprintf (stderr, "F\n");
	exit (2);
    } else fprintf (stderr, "P");

    modtest = uCdl_sym (handle, "modtest");
    if (!modtest) {
	fprintf (stderr, "F\n");
	exit (3);
    } else fprintf (stderr, "P");
    if ((*modtest)() != 0xc0de4242) {
	fprintf (stderr, "F\n");
	exit (4);
    } else fprintf (stderr, "P");

    if (called != 1) {
	fprintf (stderr, "F (%d)\n", called);
	exit (6);
    } else fprintf (stderr, "P");

    uCdl_close (handle);
    fprintf (stderr, "P\n");
    fprintf (stderr, "All tests passed.\n");
    return 0;
}
