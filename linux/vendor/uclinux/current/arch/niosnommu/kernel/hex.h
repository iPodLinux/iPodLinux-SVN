#if 1
#define outchar(X) { \
		while (!(nasys_printf_uart->np_uartstatus & np_uartstatus_trdy_mask)); \
		nasys_printf_uart->np_uarttxdata = (X); }
#else
#define outchar(X) putchar(X)
#endif
#define outhex(X,Y) { \
		unsigned long __w; \
		__w = ((X) >> (Y)) & 0xf; \
		__w = __w > 0x9 ? 'A' + __w - 0xa : '0' + __w; \
		outchar(__w); }
#define outhex8(X) { \
		outhex(X,4); \
		outhex(X,0); }
#define outhex16(X) { \
		outhex(X,12); \
		outhex(X,8); \
		outhex(X,4); \
		outhex(X,0); }
#define outhex32(X) { \
		outhex(X,28); \
		outhex(X,24); \
		outhex(X,20); \
		outhex(X,16); \
		outhex(X,12); \
		outhex(X,8); \
		outhex(X,4); \
		outhex(X,0); }

