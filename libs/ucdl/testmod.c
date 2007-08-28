extern int uCdl_init (const char *f);
extern void *uCdl_open (const char *f);
extern void callme (int param);

volatile int dontdoit = 1;

int modtest() {
	void *(*fn)(const char *) = uCdl_open;
	if (!dontdoit) {
	    uCdl_init ("error");
	    (*fn) ("error");
	}
	callme (0xdeadc0de);
	
	return 0xc0de4242;
}
