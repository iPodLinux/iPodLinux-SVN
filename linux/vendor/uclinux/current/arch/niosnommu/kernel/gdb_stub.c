// Modified for uClinux - Vic - Dec 2001
// From:

// File: nios_gdb_stub.c
// Date: 2000 June 20
// Author dvb \ Altera Santa Cruz

#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/nios.h>
#include "gdb_stub.h"

extern int _etext;

static void puts( unsigned char *s )
{
	while(*s) {
		while (!(nasys_printf_uart->np_uartstatus & np_uartstatus_trdy_mask));
		nasys_printf_uart->np_uarttxdata = *s++;
	}
}

enum
{
  na_BreakpointTrap     = 3,
  na_SingleStepTrap     = 4,
  na_StartGDBTrap       = 5
};

// --------------------------------
// Local Prototypes

#if GDB_DEBUG_PRINT
static void StringFit(char *s,int w);
#endif

// --------------------------------
// Debugging The Debugger

#if GDB_DEBUG_PRINT
void GDB_RawMessage(char *s)
	{
	StringFit(s,32);
	nr_pio_lcdwritescreen(s);
	}
#else
	#define GDB_RawMessage(a,b,c)	// define away to nothing
#endif

#if GDB_DEBUG_PRINT
void GDB_Print2(char *s,int n1,int n2)
	{
	char st[1000];

	sprintf(st,s,n1,n2);
	GDB_RawMessage(st);
	}
#else
	#define GDB_Print2(a,b,c)	// define away to nothing
#endif

// If string is longer than w, cut out the middle.

#if GDB_DEBUG_PRINT
int StringLen(char *s)
	{
	int l = 0;

	while(*s++)
		l++;
	return l;
	}

void StringFit(char *s,int w)
	{
	if(StringLen(s) > w)
		{
		int i;


		w = w / 2;

		for(i = 0; i < w; i++)
			{
			s[i + w] = s[StringLen(s) - w + i];
			}
		s[w + w] = 0;
		}
	}
#endif


// ---------------------------------------------
// Generic routines for dealing with
// hex input, output, and parsing
// (Adapted from other stubs.)


#define kTextBufferSize 2048
#define kMaximumBreakpoints 4

/*
 * This register structure must match
 * its counterpart in the GDB host, since
 * it is blasted across in byte notation.
 */
typedef struct
	{
	int r[32];
	long pc;
	short ctl0;
	short ctl1;
	short ctl2;
	short filler;
	} NiosGDBRegisters;

typedef struct
	{
	short *address;
	short oldContents;
	} NiosGDBBreakpoint;

typedef struct
	{
	NiosGDBRegisters registers;
	int trapNumber;				// stashed by ISR, to distinguish types
	char textBuffer[kTextBufferSize];
	int breakpointCount;			// breakpoints used for stepping
	NiosGDBBreakpoint breakpoint[kMaximumBreakpoints];
	} NiosGDBGlobals;

NiosGDBGlobals g = {0};		// not static: the ISR uses it!

static char dHexChars[16] = "0123456789abcdef";

/*
 * HexCharToValue -- convert a single character
 *                   to its hex value, or -1 if not.
 */
int HexCharToValue(char c)
	{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'A')
		return c - 'A' + 10;
	return -1;
	}

/*
 * HexToMem -- convert a string to a specified
 *            number of bytes in memory.
 */
char *HexToMem(char *hexIn, char *memOut, int memByteCount)
	{
	int i;
	unsigned char x;

	for(i = 0; i < memByteCount; i++)
		{
		x = HexCharToValue(*hexIn++) << 4;
		x += HexCharToValue(*hexIn++);
		*memOut++ = x;
		}

	return hexIn;
	}

/*
 * Read forward until a non-hex character, then stop.
 */

char *Hex2Value(char *hexIn, int *valueOut)
	{
	char c;
	int digitValue;
	int value = 0;

	while(1)
		{
		c = *hexIn;
		digitValue = HexCharToValue(c);
//printf("\n %c to %d \n",c,digitValue);
		if(digitValue < 0)
			{
			*valueOut = value;
			return hexIn;
			}
		hexIn++;
		value = (value << 4) + digitValue;
		}
	}

char *MemToHex(char *memIn, char *hexOut, int memByteCount)
	{
	int i;
	unsigned char x;

	for(i = 0; i < memByteCount; i++)
		{
		x = (unsigned char) *memIn++;

		*hexOut++ = dHexChars[x >> 4];
		*hexOut++ = dHexChars[x & 15];
		}

	*hexOut = 0;

	return hexOut;
	}

/*
 * Once a $ comes in, use GetGDBPacket to
 * retrieve a full gdb packet, and verify
 * checksum, and reply + or -.
 */
int GetGDBPacket(char *aBuffer)
	{
	int checksum;
	int length;
	char c;
	int x;

startPacket:
	length = 0;
	checksum = 0;

	while((c = GDBGetChar()) != '#')
		{
		if(c == '$')
			goto startPacket;

		checksum += c;
		aBuffer[length++] = c;
		aBuffer[length] = 0;
		}

	c = GDBGetChar();
	x = HexCharToValue(c) << 4;
	c = GDBGetChar();
	x += HexCharToValue(c);

	checksum &= 0xff;

	if(checksum != x)
		{
		GDBPutChar('-');
		length = 0;
		}
	else
		{
		GDBPutChar('+');
		}

	GDB_Print2("GetPacket %d",length,0);
	return length;
	}

/*
 * Send a packet, preceded by $,
 * and followed by #checksum.
 */
void PutGDBPacket(char *aBuffer)
	{
	int checksum;
	char c;

	GDBPutChar('$');
	checksum = 0;
	while((c = *aBuffer++) != 0)
		{
		checksum += c;
		GDBPutChar(c);
		}
	GDBPutChar('#');
	GDBPutChar(dHexChars[(checksum >> 4) & 15]);
	GDBPutChar(dHexChars[checksum & 15]);
	}

void PutGDBOKPacket(char *aBuffer)
	{
	aBuffer[0] = 'O';
	aBuffer[1] = 'K';
	aBuffer[2] = 0;
	PutGDBPacket(aBuffer);
	}

void DoGDBCommand_m(char *aBuffer)
	{
	char *w;
	int startAddr,byteCount;

	w = aBuffer;
	w++;				// past 'm'
	w = Hex2Value(w,&startAddr);
	w++;				// past ','
	w = Hex2Value(w,&byteCount);

//printf("\n%d , %d\n",startAddr,byteCount);

	// mA,L -- request memory

	w = aBuffer;
	w = MemToHex((char *)startAddr,w,byteCount);
	PutGDBPacket(aBuffer);
	}

void DoGDBCommand_M(char *aBuffer)
	{
	char *w;
	int startAddr,byteCount;

	w = aBuffer;
	w++;				// past 'M'
	w = Hex2Value(w,&startAddr);
	w++;				// past ','
	w = Hex2Value(w,&byteCount);
	w++;				// past ':'

	GDB_Print2("M from %x to %x",startAddr,byteCount);

	// MA,L:values -- write to memory

	w = HexToMem(w,(char *)startAddr,byteCount);

	// Send "OK"
	PutGDBOKPacket(aBuffer);
	}

// Return the values of all the registers
void DoGDBCommand_g(NiosGDBGlobals *g)
	{
	char *w;

	w = g->textBuffer;
	w = MemToHex((char *)(&g->registers),w,sizeof(g->registers));
	PutGDBPacket(g->textBuffer);
	GDB_Print2("Sent            Registers",0,0);
	}

// Set the values of all the registers
void DoGDBCommand_G(NiosGDBGlobals *g)
	{
	char *w;

	w = g->textBuffer;
	w++;	// skip past 'G'
	w = HexToMem(w,(char *)(&g->registers),sizeof(g->registers));


	// Send "OK"
	PutGDBOKPacket(g->textBuffer);

	GDB_Print2("Received        Registers",0,0);
	}

// Return last signal value
void DoGDBCommand_qm(NiosGDBGlobals *g)
	{
	char *w;

	w = g->textBuffer;

	*w++ = 'S';
	*w++ = '2';
	*w++ = '3';	// make up a signal for now...
	*w++ = 0;
	PutGDBPacket(g->textBuffer);
	}


void GDBInsertBreakpoint(NiosGDBGlobals *g,short *address)
	{
	NiosGDBBreakpoint *b;

	GDB_Print2("breakpoint 0x%x",(int)address,0);
	if(g->breakpointCount < kMaximumBreakpoints)
		{
		b = &g->breakpoint[g->breakpointCount++];
		b->address = address;
		b->oldContents = *b->address;
		*b->address = 0x7904;
		}
	}

void GDBRemoveBreakpoints(NiosGDBGlobals *g)
	{
	NiosGDBBreakpoint *b;
	int i;

	for(i = 0; i < g->breakpointCount; i++)
		{
		b = &g->breakpoint[i];
		*b->address = b->oldContents;
		b->address = 0;
		}

	g->breakpointCount = 0;
	}

int NiosInstructionIsTrap5(unsigned short instruction)
	{
	return instruction == 0x7905;
	}

int NiosInstructionIsPrefix(unsigned short instruction)
	{
	return (instruction >> 11) == 0x13;
	}

int NiosInstructionIsSkip(unsigned short instruction)
	{
	int op6;
	int op11;

	op6 = (instruction >> 10);
	op11 = (instruction >> 5);

	return (op6 == 0x14		// SKP0
		|| op6 == 0x15		// SKP1
		|| op11 == 0x3f6	// SKPRz
		|| op11 == 0x3f7	// SKPS
		|| op11 == 0x3fa);	// SKPRnz
	}

int NiosInstructionIsBranch(unsigned short instruction,short *pc,short **branchTargetOut)
	{
	int op4;
	int op7;
	int op10;
	short *branchTarget = 0;
	int result = 0;

	op4 = (instruction >> 12);
	op7 = (instruction >> 9);
	op10 = (instruction >> 6);

	if(op4 == 0x08)		// BR, BSR
		{
		int offset;

		result = 1;
		offset = instruction & 0x07ff;
		if(offset & 0x400)	// sign extend
			offset |= 0xffffF800;
		branchTarget = pc + offset + 1;	// short * gets x2 scaling automatically
		}
	else if(op10 == 0x1ff)	// JMP, CALL
		{
		result = 1;
		branchTarget = (short *)(g.registers.r[instruction & 31] * 2);
		}
	else if(op7 == 0x3d)	// JMPC, CALLC
		{
		result = 1;
		branchTarget = pc + 1 + (instruction & 0x0ffff);
#ifdef __nios32__
		branchTarget = (short *)((int)branchTarget & 0xffffFFFc);	// align 32...
#else
		branchTarget = (short *)((int)branchTarget & 0xFFFe);		// align 16...
#endif
		branchTarget = (short *)(*(int *)branchTarget);
		}

	if(branchTargetOut)
		*branchTargetOut = branchTarget;

	return result;
	}

// -------------------------
// Step at address
//
// "stepping" involves inserting a
// breakpoint at some reasonable
// spot later than the current program
// counter
//
// On the Nios processor, this is
// nontrivial. For example, we should
// not break up a PFX instruction.

void DoGDBCommand_s(NiosGDBGlobals *g)
	{
	char *w;
	int x;
	short *pc;
	short *branchTarget;
	unsigned short instruction;
	int stepType;

	/*
	 * First, if there's an argument to the packet,
	 * set the new program-counter value
	 */

	w = g->textBuffer;
	w++;
	if(HexCharToValue(*w) >= 0)
		{
		w = Hex2Value(w,&x);
		g->registers.pc = x;
		}

	/*
	 * Scan forward to see what the
	 * most appropriate location(s) for
	 * a breakpoint will be.
	 *
	 * The rules are:
	 *  1. If *pc == PFX, break after modified instruction.
	 *  2. If *pc == BR,BSR,JMP,CALL, break at destination
	 *  3. If *pc == SKIP, break right after SKIP AND after optional instruction,
	                 which might, of course, be prefixed.
	 *  4. Anything else, just drop in the breakpoint.
	 */

	pc = (short *)(int)g->registers.pc;

	instruction = *pc;
	stepType = 0;

	if(NiosInstructionIsPrefix(instruction))
		{
		/*
		 * PFX instruction: skip til after it
		 */
		while(NiosInstructionIsPrefix(instruction))
			{
			pc++;
			instruction = *pc;
			}

		GDBInsertBreakpoint(g,pc + 1);
		stepType = 1;
		}
	else if(NiosInstructionIsBranch(instruction,pc,&branchTarget))
		{
		GDBInsertBreakpoint(g,branchTarget);
		stepType = 2;
		}
	else if(NiosInstructionIsSkip(instruction))
		{
		short *pc2;
		stepType = 3;

		/*
		 * Skip gets to breaks: one after the skippable instruction,
		 * and the skippable instruction itself.
		 *
		 * Since Skips know how to skip over PFX's, we have to, too.
		 */
		pc2 = pc;	// the Skip instruction
		do
			{
			pc2++;
			} while(NiosInstructionIsPrefix(*pc2));
		// pc2 now points to first non-PFX after Skip
		GDBInsertBreakpoint(g,pc2+1);
		GDBInsertBreakpoint(g,pc+1);
		}
	else
		GDBInsertBreakpoint(g,pc+1);		// the genericest case

	GDB_Print2("Program Steppingat 0x%x (%d)",g->registers.pc,stepType);
	}

// -----------------------------
// Continue at address

void DoGDBCommand_c(NiosGDBGlobals *g)
	{
	char *w;
	int x;
	w = g->textBuffer;

	w++;		// past command

	// Anything in the packet? if so,
	// use it to set the PC value

	if(HexCharToValue(*w) >= 0)
		{
		w = Hex2Value(w,&x);
		g->registers.pc = x;
		}

	GDB_Print2("Program Running at 0x%x",g->registers.pc,0);
	}

// ----------------------
// Kill

void DoGDBCommand_k(NiosGDBGlobals *g)
	{
	return;
	}


/*
 * If we've somehow skidded
 * to a stop just after a PFX instruction
 * back up the program counter by one.
 *
 * That way, we can't end up with an accidentally-unprefixed
 * instruction.
 *
 * We do this just before we begin running
 * again, so that when the host queries our
 * registers, we report the place we actually
 * stopped.
 */

void MaybeAdjustProgramCounter(NiosGDBGlobals *g)
	{
	short instruction;
	if(g->registers.pc)
		{
		instruction = *(short *)(int)(g->registers.pc - 2);
		if(NiosInstructionIsPrefix(instruction))
			g->registers.pc -= 2;
		else
			{
			// If the *current* instruction is Trap5, we must skip it!
			instruction = *(short *)(int)(g->registers.pc);
			if(NiosInstructionIsTrap5(instruction))
				g->registers.pc += 2;
			}
		}
	}

// ----------main------------

void GDBMain(void)
	{
	char c;
	int i;

	for(i = 0; i < kTextBufferSize; i++)
		g.textBuffer[i] = i;

	GDBRemoveBreakpoints(&g);

	// Send "S05" for breakpoint encountered. No other signals.

	g.textBuffer[0] = 'S';
	g.textBuffer[1] = '0';
	g.textBuffer[2] = '5';
	g.textBuffer[3] = 0;
	PutGDBPacket(g.textBuffer);

	GDB_Print2("Trap %2d         At 0x%x",
		g.trapNumber,g.registers.pc);

/* Vic  2001-11-26
 * Inform the user that they need to add the symbol file for the application
 * that is just starting up.  Display the  .text  .data  .bss  regions.
 */
	if (g.trapNumber == 5) {
		sprintf(g.textBuffer,
				"\r\n\nGDB: trap 5 at 0x%08lX", g.registers.pc);
		puts(g.textBuffer);
		if ( current->mm->start_code > _etext ) {
			sprintf(g.textBuffer,
				"\r\nGDB: Enter the following command in the nios-elf-gdb Console Window:"
				"\r\nGDB:    add-symbol-file [path]%s.abself 0x%08lX 0x%08lX 0x%08lX\r\n\n",
				current->comm,
				(unsigned long)current->mm->start_code,
				(unsigned long)current->mm->start_data,
				(unsigned long)current->mm->end_data );
		} else {
			if (current)
				sprintf(g.textBuffer,
					", kernel process: %s\r\n", current->comm );
			else
				sprintf(g.textBuffer,
					", kernel process unknown\r\n" );
		}
		puts(g.textBuffer);
	}

	while(1)
		{
		c = GDBGetChar();
		if(c == '$')
			{
			GetGDBPacket(g.textBuffer);

			GDB_Print2(g.textBuffer,0,0);

			switch(g.textBuffer[0])
				{
				case 's':
					DoGDBCommand_s(&g);
					goto startRunning;
					break;

				case 'c':	// continue
					DoGDBCommand_c(&g);

					// if the PC is something other than 0, it's
					// probably ok to exit and go there

				startRunning:
					if(g.registers.pc)
						{
						MaybeAdjustProgramCounter(&g);
						return;
						}
					break;

				case 'm':	// memory read
					DoGDBCommand_m(g.textBuffer);
					break;

				case 'M':	// memory set
					DoGDBCommand_M(g.textBuffer);
					break;

				case 'g':	// registers read
					DoGDBCommand_g(&g);
					break;

				case 'G':	//registers set
					DoGDBCommand_G(&g);
					break;

				case 'k':	//kill process
					DoGDBCommand_k(&g);
					break;

				case '?':	// last exception value
					DoGDBCommand_qm(&g);
					break;

				default:	// return empty packet, means "yeah yeah".
					g.textBuffer[0] = 0;
					PutGDBPacket(g.textBuffer);
					break;
				}
			}
		}
	}

/*
 * int main(void)
 *
 * All we really do here is install our trap # 3,
 * and call it once, so that we're living down in
 * the GDBMain, trap handler.
 */

extern int StubBreakpointHandler;
extern int StubHarmlessHandler;

void nios_gdb_install(int active)
	{
	unsigned int *vectorTable;
	unsigned int stubBreakpointHandler;
	unsigned int stubHarmlessHandler;

	g.breakpointCount = 0;
	g.textBuffer[0] = 0;

	vectorTable = (int *)nasys_vector_table;
	stubBreakpointHandler = ( (unsigned int)(&StubBreakpointHandler) ) >> 1;
	stubHarmlessHandler = ( (unsigned int)(&StubHarmlessHandler) ) >> 1;

	/*
	 * Breakpoint & single step both go here
	 */
	vectorTable[na_BreakpointTrap] = stubBreakpointHandler;
	vectorTable[na_SingleStepTrap] = stubBreakpointHandler;
	vectorTable[na_StartGDBTrap] = active ? stubBreakpointHandler : stubHarmlessHandler;
	}

#ifdef nios_gdb_breakpoint
	#undef nios_gdb_breakpoint
#endif

void nios_gdb_breakpoint(void)
	{
	/*
	 * If you arrived here, you didn't include
	 * the file "nios_peripherals.h", which
	 * defines nios_gdb_breakpoint as a
	 * macro that expands to TRAP 5.
	 *
	 * (No problem, you can step out
	 * of this routine.)
	 */
	asm("TRAP 5");
	}

// end of file
