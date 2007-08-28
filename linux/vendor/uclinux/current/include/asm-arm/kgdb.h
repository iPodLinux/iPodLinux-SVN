/*
 * include/asm-arm/kgdb.h
 *
 * ARM KGDB support 
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (C) 2002 MontaVista Software Inc.
 *
 */

#ifndef __ASM_KGDB_H__
#define __ASM_KGDB_H__


#ifdef CONFIG_KGDB

/*
 * Generic kgdb entry point
 *
 * This is called by various trap handlers when all else fails
 * and a kernel excpetion has occured.
 */
void do_kgdb(struct pt_regs *, unsigned char);


/*
 * Determine if KGDB is running or not
 */
int kgdb_active(void);

/*
 * Are we connected to a remote client?
 */
int kgdb_connected(void);

/* 
 * kgdb_handle_bus_error() is used to recover from bus errors
 * which may occur during kgdb memory read/write operations.
 * Before performing a memory read/write operation, we set
 * the kgdb_fault_expected flag, call setjmp to save the
 * current machine context, and then perform the read/write
 * operation. If an error occurs, kgdb_handle_bus_error is
 * invoked from do_page_fault and we call longjmp to restore
 * the machine state and return to kgdb at the setjmp return
 * address. Both setjmp and longjmp return a flag which is
 * tested upon return to determine if all is well. Upon the
 * initial call to setjmp, the machine context is saved and
 * a return value of zero indicates all is well. Next, we
 * perform the read/write operation. Now, if an error occurs,
 * longjmp will return to the setjmp == 0 test case. But
 * this time, non-zero status is returned. In which case,
 * we return an error status message to the gdb host. Else,
 * if the read/write operation succeeds, we return the usual
 * status message to the gdb host.
 */
extern void kgdb_handle_bus_error(void);                                                                                              
extern int kgdb_setjmp(int *machine_context);
extern int kgdb_longjmp(int *machine_context, int flag);
extern int kgdb_fault_expected;
 
/*
 * breakpoint() is used by the kernel at initialization to force
 * a sync with the GDB on the host side.
 */
#define breakpoint()	asm (".word	0xe7ffdeff")

extern void kgdb_get_packet(unsigned char *, int);
extern void kgdb_put_packet(unsigned char *);

extern int kgdb_io_init(void);

#ifdef CONFIG_KGDB_SERIAL

extern unsigned char kgdb_serial_getchar(void);
extern void kgdb_serial_putchar(unsigned char);

extern void kgdb_serial_init(void);

#endif

#ifdef CONFIG_KGDB_UDP

extern unsigned kgdb_net_rx(unsigned char *);
extern unsigned kgdb_net_tx(unsigned char *);

#endif

#else // NO KGDB

#define	kgdb_active()	0

#define	breakpoint()

#endif 

#endif // __ASM_KGDB_H__
