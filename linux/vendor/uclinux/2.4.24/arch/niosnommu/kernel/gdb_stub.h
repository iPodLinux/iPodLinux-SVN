// Modified for uClinux - Vic - Dec 2001
// From:

// file: nios_gdb_stub.h
// Author: Altera Santa Cruz \ 2000
//
// You can modify this header file to
// enable some features useful for
// debugging the debugger. They're
// good features also to just show
// signs of life on your Nios board.
// But they consume valuable peripherals!
//
// The 'GDB_DEBUG_PRINT' option ties
// up the LCD living on the 5v port,
// showing useful internals of the stub.
//
// dvb@altera.com
//

#ifndef GDB_DEBUG_PRINT
	#define GDB_DEBUG_PRINT 0
#endif

//vic void GDB_Main(void);	// initialize gdb and begin.

//vic void GDB_Print2(char *s,int v1,int v2);


// Single character I/O for Nios GDB Stub

#ifdef nasys_gdb_uart
	#define GDB_UART nasys_gdb_uart
#endif

static inline char GDBGetChar(void)
	{
	char c = 0;
#ifdef GDB_UART
	while (!(GDB_UART->np_uartstatus & np_uartstatus_rrdy_mask));
	c = GDB_UART->np_uartrxdata;
#endif
	return c;
	}

static inline void GDBPutChar(char c)
	{
#ifdef GDB_UART
	while (!(GDB_UART->np_uartstatus & np_uartstatus_trdy_mask));
	GDB_UART->np_uarttxdata = c;
#endif
	}

