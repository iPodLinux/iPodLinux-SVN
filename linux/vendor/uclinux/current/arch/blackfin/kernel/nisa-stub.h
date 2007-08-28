/*
  GDB stub for BlackFin DSP (in emulation mode).

  Copyright (c) 2001 by Tony Kou of Arcturus Network Com.
  All right reserved.           tonyko@arcturusnetworks.com
 
  Permission to use, copy and distribute this file
  is freely granted, as long as this copyright notice
  is preserved.
 
  This file is provided "as-is", and without any express
  or implied warranties, including, without limitation,
  the implied warranties of merchantability and fitness
  for a particular purpose.
*/ 

/*
  This implementation assumes following:

	Application being debugged is always in supervisor mode. User mode is
	currently not supported.

	Assumes serial port has already been setup for appropriate baud rate.
	Tested using rate of 115200.

	Application being debugged is in ROM (eg FLASH). This is not a requirement,
	but simply the reason why this stub was written in the first place. The
	standard 68K stub would probably work (once serial port stuff was sorted)
	if application is in RAM.

	This stub supports only a single hardware breakpoint. If external
	hardware is used then you would need to modify this stub anyway. This does
	not affect the number of software breakpoints (breakpoints in RAM). GDB
	handles this by simply reading and writing to RAM (to read op-codes and
	replace them with breakpoint instructions).

	I have tried to determine if GDB needs more than one breakpoint when single
	steping through source code. I my limited testing I have not found the GDB
	needs more than one breakpoint which means that the stub can be used	to
	single step code. The limitation of one hardware breakpoint means that you
	can only set a single breakpoint	when running the application from ROM. If
	you attempt to set more than one breakpoint then this stub simply
	overwrites the any previous breakpoint with the new one, such that only
	the last breakpoint set will actually be active. Of course GDB wont know
	this so it will still think you have multiple breakpoints set.

	Have not check out how hardware breakpoints are set and used directly from
	GDB UI. Currently only tested against software breakpoints being fudged by
	this stub setting hardware breakpoints. I think I may need later version
	of GDB to test this against.
*/

/* platform-specific stuff */
void	gdb_putc ( char c );
char	gdb_getc ( void );
short	gdb_peek_register_file ( short id, long *val );
short	gdb_poke_register_file ( short id, long val );
void	gdb_step ( unsigned long addr );
void	gdb_continue ( unsigned long addr );
void	gdb_kill( void );
void	gdb_return_from_exception( void );
void	gdb_init(void);
void	gdb_set_hw_break(unsigned long addr);
int	gdb_is_rom_addr(long addr);
void	gdb_write_rom(long len,long addr, const char *hargs);


/* platform-neutral stuff */
long	hex_to_long ( char h );
char	nibble_to_hex ( char i );
long	hexbuf_to_long ( short len, const char *hexbuf );
short	long_to_hexbuf ( long l, char *hexbuf, short pad );
void	gdb_putmsg ( char c, const char *buf, short len );
short	gdb_getmsg ( char *rxbuf );
void	gdb_last_signal ( short sigval );
void	gdb_expedited ( short sigval );
void	gdb_read_memory ( const char *hargs );
void	gdb_write_memory ( const char *hargs );
void	gdb_console_output( short len, const char *buf );
void	gdb_write_registers ( char *hargs );
void	gdb_read_registers ( char *hargs );
void	gdb_write_register ( char *hargs );
void	gdb_monitor ( short sigval );
void	gdb_handle_exception( long sigval );
unsigned char gdb_putstr ( short len, const char *buf );


void gdb_exception_isr(void);

/* Function prototype for above interrupt handler */
void gdb_int7_isr(void);
void gdb_aline_isr(void);

#define BYTE_REF(addr)           (*((volatile unsigned char*)addr))
#define HALFWORD_REF(addr)       (*((volatile unsigned short*)addr))
#define WORD_REF(addr)           (*((volatile unsigned long*)addr)) 

/********************************************

	BlackFin - periperal register defintions
	---------------------------------------
*********************************************/

/* Base address of system MMR registers(2MB) FFC00000 -- FFDFFFFF */
#define	BLACKFIN_BASE	0xFFC00000

/* Interrupt Registers */
#define IMASK_ADDR                      0xffe02104 /*  register 32 bit */
#define IMASK                           WORD(IMASK_ADDR)
#define IPEND_ADDR                      0xffe02108 /*  register 32 bit */
#define IPEND                           WORD(IPEND_ADDR)
#define ILAT_ADDR                       0xffe0210c /*  register 32 bit */
#define ILAT                            WORD(ILAT_ADDR) 

/* types of interrupts *******************************/
#define EZ328_INT_EMIQ	0x00800000
#define EZ328_INT_SAM	0x00400000
#define EZ328_INT_IRQ5	0x00100000
#define EZ328_INT_IRQ6	0x00080000
#define EZ328_INT_IRQ3	0x00040000
#define EZ328_INT_IRQ2	0x00020000
#define EZ328_INT_IRQ1	0x00010000
#define EZ328_INT_INT3	0x00000800
#define EZ328_INT_INT2	0x00000400
#define EZ328_INT_INT1	0x00000200
#define EZ328_INT_INT0	0x00000100
#define EZ328_INT_PWM	0x00000080
#define EZ328_INT_KEYB	0x00000040
#define EZ328_INT_RTC	0x00000010
#define EZ328_INT_WDT	0x00000008
#define EZ328_INT_UART	0x00000004
#define EZ328_INT_TMR	0x00000002
#define EZ328_INT_SPI	0x00000001

/********************************************

	BlackFin - UART0 register defintions
	---------------------------------------
*********************************************/
#define UART0_THR_ADDR                  0xffc01800  /* UART 0 Transmit holding register  16 bit */
#define UART0_THR                       HALFWORD_REF(UART0_THR_ADDR)

#define UART0_RBR_ADDR                  0xffc01800  /* UART 0 Receive buffer register  16 bit */
#define UART0_RBR                       HALFWORD_REF(UART0_RBR_ADDR)

/* Divisor Latch Register used for calculating: Baud Rate = SCLK / (16 * DL)    */
#define UART0_DLL_ADDR                  0xffc01800  /* UART 0 Divisor latch (low byte) register  16 bit */
#define UART0_DLL                       HALFWORD_REF(UART0_DLL_ADDR)
#define UART0_DLH_ADDR                  0xffc01802  /* UART 0 Divisor latch (high byte) register  16 bit */#define UART0_DLH                       HALFWORD_REF(UART0_DLH_ADDR)

#define UART0_IER_ADDR                  0xffc01802  /* UART 0 Interrupt enable register  16 bit */
#define UART0_IER                       HALFWORD_REF(UART0_IER_ADDR)
#define UART0_IER_ERBFI                 0x01    /* Enable Receive Buffer Full Interrupt(DR bit) */
#define UART0_IER_ETBEI                 0x02    /* Enable Transmit Buffer Empty Interrupt(THRE bit) */
#define UART0_IER_ELSI                  0x04    /* Enable RX Status Interrupt(gen if any of LSR[4:1] set) */
#define UART0_IER_EDDSI                 0x08    /* Enable Modem Status Interrupt(gen if 
                                                   any UARTx_MSR[3:0] set) */

#define UART0_IIR_ADDR                  0xffc01804  /* UART 0 Interrupt identification register  16 bit */
#define UART0_IIR                       HALFWORD_REF(UART0_IIR_ADDR)
#define UART0_IIR_NOINT                 0x01    /* Bit0: cleared when no interrupt */
#define UART0_IIR_STATUS                0x06    /* mask bit for the status: bit2-1 */
#define UART0_IIR_LSR                   0x06    /* Receive line status */
#define UART0_IIR_RBR                   0x04    /* Receive data ready */
#define UART0_IIR_THR                   0x02    /* Ready to transmit  */
#define UART0_IIR_MSR                   0x00    /* Modem status       */ 

#define UART0_LCR_ADDR                  0xffc01806  /* UART 0 Line control register  16 bit */
#define UART0_LCR                       HALFWORD_REF(UART0_LCR_ADDR)
#define UART0_LCR_WLS5                  0       /* word length 5 bits */
#define UART0_LCR_WLS6                  0x01    /* word length 6 bits */
#define UART0_LCR_WLS7                  0x02    /* word length 7 bits */
#define UART0_LCR_WLS8                  0x03    /* word length 8 bits */
#define UART0_LCR_STB                   0x04    /* StopBit: 1: 2 stop bits for non-5-bit word length
                                                               1/2 stop bits for 5-bit word length
                                                            0: 1 stop bit */
#define UART0_LCR_PEN                   0x08    /* Parity Enable 1: for enable */
#define UART0_LCR_EPS                   0x10    /* Parity Selection: 1: for even pariety
                                                                     0: odd parity when PEN =1 & SP =0 */
#define UART0_LCR_SP                    0x20    /* Sticky Parity: */
#define UART0_LCR_SB                    0x40    /* Set Break: force TX pin to 0 */
#define UART0_LCR_DLAB                  0x80    /* Divisor Latch Access */

#define UART0_LSR_ADDR                  0xffc0180a  /* UART 0 Line status register  16 bit */
#define UART0_LSR                       HALFWORD_REF(UART0_LSR_ADDR)
#define UART0_LSR_DR                    0x01    /* Data Ready */
#define UART0_LSR_OE                    0x02    /* Overrun Error */
#define UART0_LSR_PE                    0x04    /* Parity Error  */
#define UART0_LSR_FE                    0x08    /* Frame Error   */
#define UART0_LSR_BI                    0x10    /* Break Interrupt */
#define UART0_LSR_THRE                  0x20    /* THR empty, REady to accept */
#define UART0_LSR_TEMT                  0x40    /* TSR and UARTx_thr both empty */

/****************************
 *
 *  WATCHPOINT & PATCH UNIT REGISTERS  (0XFFC07000 - 0XFFC07200)
 *
 ****************************/
 
#define WPIACTL_ADDR                    0xffe07000 /*  register 32 bit */
#define WPIACTL                         WORD(WPIACTL_ADDR)
                /* TOTAL OF 6  */
#define WPIA_ADDR                       0xffe07040 /*  register 32 bit */
#define WPIA                            WORD(WPIA_ADDR)
                /* TOTAL OF 6  */
#define WPIACNT_ADDR                    0xffe07080 /*  register 32 bit */
#define WPIACNT                         WORD(WPIACNT_ADDR)
 
#define WPDACTL_ADDR                    0xffe07100 /*  register 32 bit */
#define WPDACTL                         WORD(WPDACTL_ADDR)
                /* TOTAL OF 2 */
#define WPDA_ADDR                       0xffe07140 /*  register 32 bit */
#define WPDA                            WORD(WPDA_ADDR)
                /* TOTAL OF 2 */
#define WPDACNT_ADDR                    0xffe07180 /*  register 32 bit */
#define WPDACNT                         WORD(WPDACNT_ADDR)
 
#define WPSTAT_ADDR                     0xffe07200 /*  register 32 bit */
#define WPSTAT                          WORD(WPSTAT_ADDR) 





/* ICEM registers */
#define	EZ328_ICEMACR	(*(volatile unsigned long *)(EZ328BASE+0xD00))	/* ICEM address compare reg */
#define	EZ328_ICEMAMR	(*(volatile unsigned long *)(EZ328BASE+0xD04))	/* ICEM address mask reg */
#define	EZ328_ICEMCCR	(*(volatile unsigned short *)(EZ328BASE+0xD08))	/* ICEM control compare reg */
#define	EZ328_ICEMCMR	(*(volatile unsigned short *)(EZ328BASE+0xD0A))	/* ICEM control mask reg */
#define	EZ328_ICEMCR	(*(volatile unsigned short *)(EZ328BASE+0xD0C))	/* ICEM control reg */
#define	EZ328_ICEMSR	(*(volatile unsigned short *)(EZ328BASE+0xD0E))	/* ICEM status reg */

/****************************
 *
 *  WATCHDOG TIMER  (0XFFC01000 - 0XFFC013F) time = COUNT * SCLK
 *
 ****************************/
 
#define WDOGCTL_ADDR                    0xffc01000  /* Watchdog control register  32 bit */
#define WDOGCTL                         WORD_REF(WDOGCTL_ADDR)
#define WDOGCNT_ADDR                    0xffc01004  /* Watchdog count register 32 bit */
#define WDOGCNT                         WORD_REF(WDOGCNT_ADDR)
#define WDOGSTAT_ADDR                   0xffc01008  /* Watchdog status register 32 bit */
#define WDOGSTAT                        WORD_REF(WDOGSTAT_ADDR) 

/* Define values for Watchdog register */
#define WDOGCTL_EN_GP		0x04 /* watchdog invoke GP interrupt: IVG13 */
#define WDOGCTL_ENABLE		0xF0 /* 0000-1000 and 1110-1111 of Bit: 7-4 enable it */
#define WDOGCTL_DISABLE		0xE0 /* only 1101 of Bit: 7-4 disable WDOG */

/*********************

	BlackFin - vectors
	-----------------
**********************/

/* Macro function to set a vector into the vector table */
#define SET_VECTOR(vect,func)  	*(unsigned long*)((unsigned long)vect) = (unsigned long)func

/* Invoked by: Watchdog, 
 * Note: shared with Memory DMA interrupt.
 */
#define EVT_IVG13_ADDR		0xffe02034 /*  General purpose interrupt register 32 bit */
#define EVT_IVG13		WORD(EVT_IVG13_ADDR) 

/* Inviked by: UART0 RCV/XMT or UART1 RCV/XMT    */
#define EVT_IVG10_ADDR		0xffe02028 /*  General purpose interrupt register 32 bit */
#define EVT_IVG10		WORD(EVT_IVG10_ADDR)

/* Invoked by: 
 *    EXCAUSE = 0b010000 :  Single Step
 *              0b010001 :  Trace Buffer Overflow  
 *              0b101000 :  Emulation Watch Point Match
 *              0000-1111:  EXCPT m, and "EXCPT 15" as SW breakpoint
 */
#define EVT_EXCEPTION_ADDR	0xffe0200c /* Identification with code in EXCAUSE register 32 bit */
#define EVT_EXCEPTION		WORD(EVT_EXCEPTION_ADDR)
#define STEP_BIT		0x02	/* step-bit in SYSCFG register */

/**************************************

	BlackFin - CPU register defintions
	---------------------------------
***************************************/

/*
	There are 51 registers under BlackFin DSP core, for convinience, we treat
	extended accumulator A0x and A1x as 32-bit reg, though only 8-bit long.
	The D0-D7,A0-A7,PS,PC registers are 4 unsigned char (32 bit)
*/
#define NUMREGBYTES 51*4
typedef enum
{
  R0, R1, R2, R3, R4, R5, R6, R7,
  P0, P1, P2, P3, P4, P5, SP, FP, USP,
  I0, I1, I2, I3, M0, M1, M2, M3, 
  L0, L1, L2, L3, B0, B1, B2, B3,
  A0x, A0w, A1x, A1w,
  ASTAT, RETS, RETI, RETX, RETN, RETE,
  LC0, LC1, LT0, LT1, LB0, LB1,
  SYSCFG, SEQSTAT
} regname;

long CPUregisters[NUMREGBYTES/4];

/*************************

	Local stack for stub
	--------------------
**************************/
/*
	FIXME: Currently not used as code crashes on switching to local stack
	Reason for needing local stack - Need to consider affect of "inferior" function 
        calls on application stack
	When I really figure out what this really means I may look at fixing this.
*/
#define STACKSIZE 400
int localStack[STACKSIZE/sizeof(int)];
int* localStackPtr = &localStack[STACKSIZE/sizeof(int) - 1];


/*************************

	Useful Macro functions
	----------------------
**************************/

/* Disable all possible interrupts levels (except NMI, RESET, EMU and EVSW) */
#define DISABLE_ALL_INTERRUPTS(save)	asm("	%0 = cli;\n\t": "=d" (save));

/* Enable all interrupts levels */
#define ENABLE_ALL_INTERRUPTS(save)	asm("	sti %0;\n\t": : (d) (save));

/* disable further emulator interrupts - esp level 7*/
#define DISABLE_EMU_INTERRUPTS()	asm("	ori.l #0x00800000,0xFFFFF304");



/* SW breakpoint: hope no confilict with others : opcode: 0x00AF(2 Bytes) */
#define BREAKPOINT()	asm("	excpt 15;\n\t")


/* Value related with setting up WP MMR  */
static const unsigned long ENABLE_INSN_WP[4]=
	{0x29, 0x51, 0x2801, 0x5001};

static const unsigned long ENABLE_INSN_WP_MASK[4]=
	{0xFDE7F950, 0xFDE7F8A8, 0xFDE751F8, 0xFDE6A9F8};

/* Value about data address watchpoint MMR registers */
static const unsigned long ENABLE_DATA_WP_MASK[2]=
	{ 0xFFFFFC2A, 0xFFFFC3D6}

static const unsigned long ENABLE_DATA_WP[2]=
	{ 0x54, 0x828}


/* enable instruction address watchpoint 1-4 */
#define ENABLE_INST_WATCHPOINT(num, addr)					\
	(*((volatile unsigned long*)(WPIACNT_ADDR + num * 4))) &= 0xFFFF0000;	\
	(*((volatile unsigned long*)(WPIA_ADDR + num * 4))) = (addr & (~1));	\
	WPIACTL = (WPIACTL & ENABLE_INSN_WP_MASK[num]) | ENABLE_INSN_WP[num];
	
#define DISABLE_INST_WATCHPOINT(num, addr)	\
	WPIACTL &= ~ENABLE_INSN_WP[num]



/*
  Generic code to save processor context.
  Assumes the stack looks like this:

  ....				<--- stack pointer + 6
  pc(32 bits)			<--- stack pointer + 2
  sr(16 bits)			<--- stack pointer points here
*/
#define SAVE_CONTEXT()			asm("
	[--sp] = (r7:7, p5:5);
	p5.l = CPUregisters;
	p5.h = CPUregisters;
	[p5++] = r0;
	[p5++] = r1;
	[p5++] = r2;
	[p5++] = r3;
	[p5++] = r4;
	[p5++] = r5;
	[p5++] = r6;
	[p5++] = r7;
	[p5++] = p1;
	[p5++] = p2;
	[p5++] = p3;
	[p5++] = p4;
	r7 = [sp];	/* get original p5 */
	[p5++] = r7;
	r7 = sp;
	r7 += -8;	/* get original sp */
	[p5++] = r7;
	[p5++] = fp;
	r7 = usp;
	[p5++] = r7;

	r7 = i0;
	[p5++] = r7;
	r7 = i1;
	[p5++] = r7;
	r7 = i2;
	[p5++] = r7;
	r7 = i3;
	[p5++] = r7;

	r7 = m0;
	[p5++] = r7;
	r7 = m1;
	[p5++] = r7;
	r7 = m2;
	[p5++] = r7;
	r7 = m3;
	[p5++] = r7;

	r7 = l0;
	[p5++] = r7;
	r7 = l1;
	[p5++] = r7;
	r7 = l2;
	[p5++] = r7;
	r7 = l3;
	[p5++] = r7;
		

	r7 = b0;
	[p5++] = r7;
	r7 = b1;
	[p5++] = r7;
	r7 = b2;
	[p5++] = r7;
	r7 = b3;
	[p5++] = r7;

	r7 = a0x;
	[p5++] = r7;
	r7 = a0w;
	[p5++] = r7;
	r7 = a1x;
	[p5++] = r7;
	r7 = a1w;
	[p5++] = r7;

	r7 = ASTAT;
	[p5++] = r7;
	r7 = RETS;
	[p5++] = r7;
	r7 = RETI;
	[p5++] = r7;
	r7 = RETX;
	[p5++] = r7;	/* get breakpoint from here, former pc */
	r7 = RETN;
	[p5++] = r7;
	r7 = RETE;
	[p5++] = r7;
	r7 = LC0;
	[p5++] = r7;
	r7 = LC1;
	[p5++] = r7;
	r7 = LT0;
	[p5++] = r7;
	r7 = LT1;
	[p5++] = r7;
	r7 = LB0;
	[p5++] = r7;
	r7 = LB1;
	[p5++] = r7;
	r7 = SYSCFG;
	[p5++] = r7;
	r7 = SEQSTAT;
	[p5++] = r7;

	(r7:7, p5:5) = [sp++];

/*	sp.l = localStackPtr;
	sp.h = localStackPtr;	*//* switch to local stack - not currently working so commented out! */
");

/*
  Stuffs a unsigned char out the serial port.
 */
void gdb_putc ( char c )
{
	unsigned int temp, temp_uartctl;

	DISABLE_ALL_INTERRUPTS(temp)
	/* Cant allow UART interrupts while sending */
	temp_uartctl = UART0_IER;
	UART0_IER = 0;

	/* wait for space in UART tx  */
	while (!(UART0_LSR & UART0_LSR_THRE));

	/* place unsigned char from buffer into tx  */
	UART0_THR =  (unsigned short)c;

	/*
		Allow UART interrupts now that have finished
	*/
	UART0_IER = temp_uartctl | UART0_IER_ETBEI | UART0_IER_ELSI;

	ENABLE_ALL_INTERRUPTS(temp)
	return;
}


/*
  Blocks until a unsigned char is received at the serial port.
 */
char gdb_getc ( void )
{
	unsigned int temp, temp_uartctl;
	char c;
	unsigned short rxData;

	DISABLE_ALL_INTERRUPTS(temp)
	/* Cant allow UART interrupts while sending */
	temp_uartctl = UART0_IER;
	UART0_IER = 0;

	/* Do initial read of uart RX register to determine if any characters have been received */
	rxStatusData = EZ328_URX;

	/* Wait for received data */
	while ( !(rxData & (UART0_LSR | UART0_LSR_DR ))
	{
	   rxData = UART0_RBR;

           /* Tickle H/W watchdog timer *********************/
	EZ328_WATCHDOG = EZ328_WATCHDOG_ENABLE;
	}

	/* Assign char received to variable to return - ignore any errors during receive */
	c = (char)rxStatusData;

	/*
		Allow UART interrupts now that have finished
	*/
	UART0_IER = temp_uartctl | UART0_IER_ERBFI | UART0_IER_ELSI;

	ENABLE_ALL_INTERRUPTS(temp)
	return c;
}


/*
  Retrieves a register value from gdb_register_file.
  Returns the size of the register, in bytes,
  or zero if an invalid id is specified (which *will*
  happen--- gdb.c uses this functionality to tell
  how many registers we actually have).
 */
short gdb_peek_register_file ( short id, long *val )
{
	/* all supported registers are int (4Bytes) */
	short retval = sizeof( long );

	if ( (id >= R0) && (id <= SEQSTAT) )
	{
		*val = CPUregisters[id];
	}
	else
	{
		retval = 0;
	}

  return retval;
}


/*
  Stuffs a register value into gdb_register_file.
  Returns the size of the register, in bytes,
  or zero if an invalid id is specified.
 */
short gdb_poke_register_file ( short id, long val )
{
	/* all our registers are longs */
	short retval = sizeof( long );

	if ( (id >= R0) && (id <= SEQSTAT) )
	{
		CPUregisters[id] = val;
	}
	else
	{
		retval = 0;
	}

	return retval;
}


/*
  Uses the UBC to generate an exception
  after we execute the next instruction.
*****************************************************/
void gdb_step ( unsigned long addr )
{

	/* if we're stepping from an address,
           adjust pc (untested!) */
  	/* TODO: test gdb_step when PC is supplied */
	if( addr )
	{
           CPUregisters[RETX] = addr;
	}

	/* set Trace bit in CPU status register */
	CPUregisters[SYSCFG] |= STEP_BIT;

	/* we're all done now */
  	return;
}


/*
  Continue program execution at addr,
  or at the current pc if addr == 0.
*/
void gdb_continue ( unsigned long addr )
{

  if( addr )
  {
    CPUregisters[RETX] = addr;
  }

  /* Clear Trace bit in CPU status register - in case been tracing */
  CPUregisters[SYSCFG] &= ~STEP_BIT;

  return;
}


/*
  Kills the current application.
  Simulates a reset by jumping to
  the address taken from the reset
  vector at address 0.
 */
void gdb_kill ( void )
{
}

/* Flag for insn & data watchpoint MMR registers:  */
static unsigned int cur_watch_points = 0;
static unsigned int cur_watch_range = 0;
static unsigned int cur_data_points = 0;


typedef struct
{	unsigned long insn_type[4];
	unsigned long insn_addr[4];

	unsigned long data_mode[2];
	unsigned long data_addr[2];

	unsigned long insn_range_addr;
	unsigned long insn_range_len;

	unsigned long data_range_type;
	unsigned long data_range_addr;
	unsigned long data_range_len;
}watch_point_t;

static watch_point_t cur_watch={
	{-1,-1,-1,-1},
	{0,  0, 0, 0},
	{-1, -1},{0 , 0},
	0, 0, -1, 0, 0};

void gdb_remove_watch(
	unsigned long type, 
	unsigned long addr, 
	unsigned long length)
{
     unsigned char i;

     switch (type) {
	case 1: /* remove hardware breakpoint */
		if (length && cur_watch_range && (cur_watch.insn_range_addr == addr)
				&& (cur_watch.insn_range_len == length)){
		    cur_watch_range = 0;
		    WPIACTL &= ~0x220000; 
		 }

		if (!length && cur_watch_points){
		    i = 0;
		    while ((cur_watch.insn_type[i] == -1) || (cur_watch.insn_addr[i] != addr) && i<4)
			i++;

		    if (i < 4){
			cur_watch.insn_type[i] = -1;
			cur_watch_points--;
			DISABLE_INST_WATCHPOINT(i, addr);  
		     }
		 }
		break;

	case 2:  /* remove data hardware address watchpoint */
	case 3:
	case 4:
		if (length && cur_data_points == 3 && data_range_type == type
			&& data_range_addr == addr && data_range_len == length) {
		    cur_data_points = 0;
		    data_range_type = -1;

		    WPDACTL &= ~0x0D;
		 }

		if (!length && cur_data_points && cur_data_points < 3) {
		    i = 0;
		    while (cur_watch.data_mode[i] == -1 || 
				cur_watch.data_addr[i] != addr || i<2) i++;
		    if (i < 2) {
		    	cur_watch.data_mode[i] = -1;
		    	cur_data_points--;

		    	WPDACTL &= ~(i? 0x9 : 0x5);
		     }
		}
		break;
		

	case 0: /* remove software breakpoint */
	} /* end of switch (type)  */

    return;
}


void gdb_insert_watch(
	unsigned long type,
	unsigned long addr, 
	unsigned long length)
{
     unsigned char i, mode;

     switch(type){
	case 1:	/* set hardware breakpoint */
		if (length && !cur_watch_range){
		    cur_watch_range =1;
		    cur_watch.insn_range_addr = addr;
		    cur_watch.insn_range_len = length;

		    /* enable instruction address range watchpoint to WPIA45 */
		    (*((volatile unsigned long*)(WPIA_ADDR + 4*4))) = addr & (~1); 
		    (*((volatile unsigned long*)(WPIA_ADDR + 5*4))) = (addr+length) & (~1); 
		    (*((volatile unsigned long*)(WPIACNT_ADDR + 4*4))) = 0;
		    WPIACTL = (WPIACTL & 0xFD81FDFC) | 0x220001;
		 }

		if (! length && (cur_watch_points < 4)){
		     i = 0;
		     while (cur_watch.insn_type[i] != -1 && i<4) i++;
		     if ( i < 4){
		     	cur_watch.insn_type[i] = 1;
		     	cur_watch.insn_addr[i] = addr;
		     	cur_watch_points++;

		     	ENABLE_INST_WATCHPOINT(i, addr); }
		  }

		break;

	case 2:	/* data hardware address watchpoint */
	case 3:
	case 4:
		if (length && !cur_data_points){ /* ok to set up data addr WP */
		    cur_data_points = 3; /* no way to set seperate data addr WP */
		    data_range_type = type;
		    data_range_addr = addr;
		    data_range_len = length;

		    (*((volatile unsigned long*)WPDA_ADDR)) = addr;
		    (*((volatile unsigned long*)(WPDA_ADDR + 4))) = addr + length;
		    WPDACNT &= 0xFFFF0000; /* only need set up WPDACNT0  */
		    WPDACTL = (WPDACTL & 0xFFFFF020) | 0x851 | ((type-1) << 8);
		}

		if (!length && cur_data_points < 2) {
		    i = 0;
		    while (cur_watch.data_mode[i] != -1 && i < 2) i++;
		    if (i < 2) {
			cur_watch.data_mode[i] = 1;
			cur_watch.data_addr[i] = addr;
			cur_data_points++;

		    (*((volatile unsigned long*)(WPDA_ADDR + i*4))) = addr;
		    (*((volatile unsigned long*)(WPDACNT_ADDR + i*4))) &= 0xFFFF0000;
		    WPDACTL = (WPDACTL & ENABLE_DATA_WP_MASK[i]) | ENABLE_DATA_WP[i] 
				| ((type-1) << (i? 12 : 8);
		     }
		 }
		break;
		     
	caes 0: /* software breakpoint */

	}

    return; 
}   



/*
	EXCEPTION ISR (Single step/software breakpoint: EXCPT 15)
*/
asm("
.align 2
.global gdb_exception_isr;
.type gdb_exception_isr, STT_FUNC;
gdb_exception_isr:
");
	SAVE_CONTEXT();

	//DISABLE_EMU_INTERRUPTS();  ***************

asm("
	/* Put SIGTRAP on R0 as arg of gdb_handle_exception(long sigval) */
	R1 = SEQSTAT;
	R1 <<= 26;
	R1 >>= 26;	/* get the EXCAUSE field */
	R0 = 0xf(Z);	/* EXCPT 15 happen */
	CC = R1 == R0;
	BRT 1f;

	R0 = 0x10(Z);	/* single step */
	CC = R1 == R0;
	BRT 1f;

	R0 = 0x28(Z);	/* Watchpoint match invoked*/
	CC = R1 == R0;
	BRF gdb_return_from_exception;
1:	
	R0 = 5(Z);
	call	 gdb_handle_exception;	/* never returns to here! */

/*
  Restores registers to the values specified in CPUregisters
*/
gdb_return_from_exception:
	p0.l = CPUregisters;
	p0.h = CPUregisters;
	p0 += 200;	// point to end of CPUregisters.
	r0 = [p0--];
	seqstat = r0;
	r0 = [p0--];
	syscfg = r0;
	r0 = [p0--];
	lb1 = r0;
	r0 = [p0--];
	lb0 = r0;
	r0 = [p0--];
	lt1 = r0;
	r0 = [p0--];
	lt0 = r0;
	r0 = [p0--];
	lc1 = r0;
	r0 = [p0--];
	lc0 = r0;
	r0 = [p0--];
	rete = r0;
	r0 = [p0--];
	retn = r0;
	r0 = [p0--];
	retx = r0;
	r0 = [p0--];
	reti = r0;
	r0 = [p0--];
	rets = r0;
	r0 = [p0--];
	astat = r0;
	
	r0 = [p0--];
	a1w = r0;
	r0 = [p0--];
	a1x = r0;
	r0 = [p0--];
	a0w = r0;
	r0 = [p0--];
	a0x = r0;

	r0 = [p0--];
	b3 = r0;
	r0 = [p0--];
	b2 = r0;
	r0 = [p0--];
	b1 = r0;
	r0 = [p0--];
	b0 = r0;

	r0 = [p0--];
	l3 = r0;
	r0 = [p0--];
	l2 = r0;
	r0 = [p0--];
	l1 = r0;
	r0 = [p0--];
	l0 = r0;

	r0 = [p0--];
	m3 = r0;
	r0 = [p0--];
	m2 = r0;
	r0 = [p0--];
	m1 = r0;
	r0 = [p0--];
	m0 = r0;

	r0 = [p0--];
	i3 = r0;
	r0 = [p0--];
	i2 = r0;
	r0 = [p0--];
	i1 = r0;
	r0 = [p0--];
	i0 = r0;

	r0 = [p0--];
	usp = r0;
	fp = [p0--];
	sp = [p0--];
	p5 = [p0--];
	p4 = [p0--];
	p3 = [p0--];
	p2 = [p0--];
	p1 = [p0--];
	r0 = [p0--];
	[--sp] = r0;	/* temp save p0 to system stack */

	r7 = [p0--];
	r6 = [p0--];
	r5 = [p0--];
	r4 = [p0--];
	r3 = [p0--];
	r2 = [p0--];
	r1 = [p0--];
	r0 = [p0--];
	p0 = [sp++];	/* restore p0 and system stack */


	andi.l #0xFF7FFFFF,0xFFFFF304 /* IMR - enable emulator interrupts */
 	move.w 0xFFFFFD0E,0xFFFFFD0E	/* ICEMSR - clear status bits */

	rtx;	  /* return from exception! */
");


void gdb_init(void)
{
	/* Set vector for step and EXCPT 15 interrupts */
   SET_VECTOR(EVT_EXCEPTION_ADDR,gdb_exception_isr);


	/* initialize watchpoint MMR: disable every watchpoints
           and enable watch point unit, also enable WPIREN */
   WPDACTL |= 0xFC020001;
   WPDACTL = 0;
   WPSTAT = 0;	/* clear WP status register */



   SET_VECTOR(TRAP_1_VECTOR,gdb_traceTrap_isr);

   /* Vectors for emulation mode */
	SET_VECTOR(INT_LEVEL_7_VECTOR,gdb_int7_isr);
	SET_VECTOR(A_LINE_VECTOR,gdb_aline_isr);


	/* Enable EMUIRQ signal to cause interrupt */
	EZ328_PGSEL &= ~0x04;
	EZ328_PGPUEN |= 0x04;

   EZ328_ICEMSR = EZ328_ICEMSR;	/* ICEMSR - clear status bits to avoid immediate interrupt */

	/* enable emulator breakpoints */
	EZ328_IMR &= ~EZ328_INT_EMIQ;

	BREAKPOINT();	/* Causes control to be passed to GDB stub */
}

int gdb_is_rom_addr(long addr)
{
	return (int)((addr >= 0x800000L) && (addr < 0xC00000L ));
}

void gdb_write_rom(long len,long addr, const char *hargs)
{
  /*
  	Test for special case of attempting to write a trap #1 op-code (0x4E41) to flash memory
	In this case do a hardware breakpoint instead
	Ignore all other cases - ie simply dont write anything.
  */
	if ( len == 2 )
	{
   	if (hexbuf_to_long( sizeof( short ) * 2, hargs ) == 0x4e41L)
      	    gdb_set_hw_break(addr);
	}
}

void gdb_set_hw_break(unsigned long addr)
{
	EZ328_ICEMACR = addr;
	EZ328_ICEMAMR = 0x00000000;	/* full address matches only */
	EZ328_ICEMCCR = 0x0003;			/* read instruction breakpoint */
	EZ328_ICEMCMR = 0x0000;			/* Dont mask read/write and data/instruction matching */
	EZ328_ICEMCR = 0x001F;			/* Enable int7 for Bus breakpoint, disable hardmap, Single breakpoint,
												program break, Enable compare */
}
short lastICEMSR;

/*
	HW breakpoint ISR
*/
asm("
   .global gdb_int7_isr
gdb_int7_isr:
");
DISABLE_EMU_INTERRUPTS();
ENABLE_ALL_INTERRUPTS();	/* A bit controversial - should interrupts be enabled after a breakpoint - remove this if not */
SAVE_CONTEXT();
asm("
	move.w 0xFFFFFD0E, %d0	/* read current ICEMSR */
   move.w %d0,lastICEMSR	/* save it  */
	btst	 #1,%d0		/* test for a-line exception - instruction breakpoint */
	beq	 clearStatus
							/* remove a-line exception frame to get at real frame */
	move.w	6(%sp),66(%a0)		/* Save SR to CPUregisters */
	move.l	8(%sp),68(%a0)		/* Save PC to CPUregisters */
	addi.l	#6,60(%a0)  	 /* fixup saved stack pointer to position prior to interrupt */

clearStatus:
 	move.w 0xFFFFFD0E,0xFFFFFD0E	/* ICEMSR - clear status bits */
	andi.w #0xFFFE,0xFFFFFD0C		/* ICEMCR - disable further breakpoint interrupts */

	/* Push SIGTRAP on STACK(local) */
	move.l #5, -(%sp)
	jsr	 gdb_handle_exception /* never returns to here! */
");

/*
	Simply return from a-line exception
	Will get an a-line exception as well as int7 exception for certains types of breakpoints.
	But get an int7 exception for all types of breakpoints, so can simply ignore all a-line exceptions.
*/
asm("
	.global gdb_aline_isr
gdb_aline_isr:
	rte
");

