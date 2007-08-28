/**
 * Generic portion for gdb stub without any architecture- or platform- specific
 * infomation.
 *
 * Copyright (c) 2001 by Tony Kou of Arcturus Network Com.
 * All right reserved. 		tonyko@arcturusnetworks.com
 *
 * This file is provided "as-is", and without any express
 * or implied warranties, including, without limitation,
 * the implied warranties of merchantability and fitness
 * for a particular purpose.
 */

/**************************************************************************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    Pnnn=regs     Write register nnn with value          OK or ENN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    zt,addr,len   Remove hardware breakpoint or 
 *                  hardware watchpoint
 *                  Suupport:
 *                    . 4 hardware breakpoint
 *                    . 1 insn address range watchpoint
 *                    . 2 data address watchpoint or 
 *                      1 data address range watchpoint    OK or ENN
 *
 *    Zt,addr,len   Insert hardware breakpoint or 
 *                  hardware watchpoint
 *                  Suupport:
 *                    . 4 hardware breakpoint
 *                    . 1 insn address range watchpoint
 *                    . 2 data address watchpoint or 
 *                      1 data address range watchpoint    OK or ENN
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.               
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/ 

#include "nisa-stub.h"

#define GDB_RXBUFLEN	300
#define GDB_TXBUFLEN	300 

const char nibble_to_hex_table[] = "0123456789abcdef";


/*
 * Converts '[0-9,a-f,A-F]'
 * to its long integer equivalent.
 */
long hex_to_long ( char h )
{
  if( h >= 'a' && h <= 'f' )
    return h - 'a' + 10;

  if( h >= '0' && h <= '9' )
    return h - '0';

  if( h >= 'A' && h <= 'F' )
    return h - 'A' + 10;

  return 0;
}


/*
 * Converts the low nibble of i
 * to its hex character equivalent.
 */
char nibble_to_hex ( char i )
{
  return nibble_to_hex_table[i & 0xf];
}


/*
 * Converts an arbitrary number of hex
 * digits to a long integer.
 */
long hexbuf_to_long ( short len, const char *hexbuf )
{
  int retval = 0;


  while( len-- )
    retval = ( retval << 4 ) + hex_to_long( *hexbuf++ );

  return retval;
}


/*
 * Converts a long integer into a string of hex bytes.
 *
 * If pad is zero, then we only consume the exact number of
 * bytes in hexbuf that we need; there will be no leading zeros.
 *  
 * If pad is nonzero, we consume exactly pad bytes (up to 8),
 * of hexbuf, left-padding hexbuf with zeros if necessary,
 * and ignoring the most significant bits of l if necessary.
 *
 * We return the number of bytes in hexbuf consumed.
 */
short long_to_hexbuf ( long l, char *hexbuf, short pad )
{
  short consumed = 0;
  signed short bits;


  if( pad )
    bits = ( pad * sizeof( long )) - sizeof( long );
  else
    bits = sizeof( long ) * 8 - 4;

  do {

    if(( *hexbuf = nibble_to_hex( l >> bits )) != '0'
       || pad || consumed ) {

      hexbuf++;
      consumed++;
    }

    bits -= 4;

  } while( bits >= 0 );

  /* always consume at least one byte, even if l was zero */
  return consumed ? consumed : 1;
}


/*
 */
unsigned char gdb_putstr ( short len, const char *buf )
{
  unsigned char sum = 0;

  
  while( len-- ) {
    sum += *buf;
    gdb_putc( *buf++ );
  }

  return sum;
}


/*
 */
void gdb_putmsg ( char c, const char *buf, short len )
{
  unsigned char sum;


  do {
    
    /* send the header */
    gdb_putc( '$' );

    /* send the message type, if specified */
    if( c ) 
      gdb_putc( c );

    /* send the data */
    sum = c + gdb_putstr( len, buf );

    /* send the footer */
    gdb_putc( '#' );
    gdb_putc( nibble_to_hex( sum >> 4 ));
    gdb_putc( nibble_to_hex( sum ));

  } while( '+' != gdb_getc() );

  return;
}


/*
 */
short gdb_getmsg ( char *rxbuf )
{
  char c;
  unsigned char sum;
  unsigned char rx_sum;
  char *buf;

  
 get_msg:

  /* wait around for start character, ignore all others */
  while( gdb_getc() != '$' );

  /* start counting bytes */
  buf = rxbuf;
  sum = 0;
  
  /* read until we see the '#' at the end of the packet */
  do {
    
    *buf++ = c = gdb_getc();
    
    if( c != '#' ) {
      sum += c;
    }
      
    /* since the buffer is ascii,
       may as well terminate it */
    *buf = 0;
    
  } while( c != '#' );
  
  /* receive checksum */
  rx_sum = hex_to_long( gdb_getc());
  rx_sum = ( rx_sum << 4 ) + hex_to_long( gdb_getc() );
  
  /* if computed checksum doesn't
     match received checksum, then reject */
  if( sum != rx_sum ) {
    
    gdb_putc( '-' );
    goto get_msg;
  }
  
  else {
    /* got the message ok */
    gdb_putc( '+' );
  }

  return 1;
}


/*
 * "last signal" message
 * 
 * "Sxx", where:
 * xx is the signal number
 */
void gdb_last_signal ( short sigval )
{
  char tx_buf[2];
  

  gdb_putmsg( 'S', tx_buf,
	      long_to_hexbuf( sigval, tx_buf, 2 ));

  return;
}


/*
 * "expedited response" message
 * "Txx..........."
 *
 */
void gdb_expedited ( short sigval )
{
  char tx_buf[sizeof( long ) * 2];
  long val;
  short id = 0;
  short reglen;
  unsigned char sum;


  do {

    /* send header */
    gdb_putc( '$' );
    sum = gdb_putstr( 1, "T" );

    /* signal number */
    sum += gdb_putstr( long_to_hexbuf( sigval, tx_buf, 2 ), tx_buf );

    /* register values */
    while(( reglen = gdb_peek_register_file( id, &val )) != 0 ) {

      /* register id */
      sum += gdb_putstr( long_to_hexbuf( id, tx_buf, 0 ), tx_buf );
      sum += gdb_putstr( 1, ":" );

      /* register value
	 (gdb 4.18 requires all 32 bits in values) */
      sum += gdb_putstr( long_to_hexbuf( val, tx_buf, reglen * 2 ), tx_buf );
      sum += gdb_putstr( 1, ";" );

      /* try the next register */
      id++;
    }

    /* send the message footer */
    gdb_putc( '#' );
    gdb_putc( nibble_to_hex( sum >> 4 ));
    gdb_putc( nibble_to_hex( sum ));

  } while( '+' != gdb_getc() );

  return;
}


/*
 */
void gdb_read_memory ( const char *hargs )
{
  char tx_buf[sizeof( long ) * 2];
  long addr = 0;
  long len = 0;
  unsigned char sum;


  /* parse address */
  while( *hargs != ',' )
    addr = ( addr << 4 ) + hex_to_long( *hargs++ );

  /* skip ',' */
  hargs++;

  /* parse length */
  while( *hargs != '#' )
    len = ( len << 4 ) + hex_to_long( *hargs++ );

  /* skip '#' */
  hargs++;

  /* send header */
  gdb_putc( '$' );
  sum = 0;
  
  while( len ) {
      
    /* read in long units where we can
       (this is important if the address is
       a peripheral register or other memory
       location that only supports long accesses) */
    while( len >= sizeof( long )
	   && ( addr % sizeof( long ) == 0 )) {
      
      sum += gdb_putstr( long_to_hexbuf( *(long*)addr, tx_buf,
					 sizeof( long ) * 2 ), tx_buf );
      
      addr += sizeof( long );
      len -= sizeof( long );
    }
    
    /* read in short units where we can
       (same reasons as above) */
    if( len >= sizeof( short )
	&& ( addr % sizeof( short ) == 0 )) {
      
      sum += gdb_putstr( long_to_hexbuf( *(short*)addr, tx_buf,
			   sizeof( short ) * 2 ), tx_buf );
      
      addr += sizeof( short );
      len -= sizeof( short );
    }
    
    if( len == 1
	|| ( len && ( addr % sizeof( short ) != 0 ))) {
      
      /* request is totally misaligned;
	 read a byte, and hope for the best */
      sum += gdb_putstr( long_to_hexbuf( *(char*)addr, tx_buf,
			   sizeof( char ) * 2 ), tx_buf );
      
      addr += sizeof( char );
      len -= sizeof( char );
    }
  }

  /* send the message footer */
  gdb_putc( '#' );
  gdb_putc( nibble_to_hex( sum >> 4 ));
  gdb_putc( nibble_to_hex( sum ));

  /* get response from gdb (ignore it for now) */
  /* TODO: check gdb response in gdb_read_memory */
  gdb_getc();

  return;
}


/*
 */
void gdb_write_memory ( const char *hargs )
{
  long addr = 0;
  long len = 0;

  /* parse address */
  while( *hargs != ',' )
    addr = ( addr << 4 ) + hex_to_long( *hargs++ );

  /* skip ',' */
  hargs++;

  /* parse length */
  while( *hargs != ':' )
    len = ( len << 4 ) + hex_to_long( *hargs++ );

  /* skip ':' */
  hargs++;

  /* Check for attempt to write to ROM - parse to target specific code to handle this one  */
  if (gdb_is_rom_addr(addr))
  {
	 gdb_write_rom(len, addr, hargs);
  }
  else
  {
     /* write in long units where we can
        (this is important if the target is
        a peripheral register or other memory
        location that only supports long accesses) */
     while( len >= sizeof( long )
   	 && addr % sizeof( long ) == 0 ) {

       *(long*)addr = hexbuf_to_long( sizeof( long ) * 2, hargs );
       hargs += sizeof( long ) * 2;
       addr += sizeof( long );
       len -= sizeof( long );
     }

     /* write in short units where we can (same reasons as above) */
     if( len >= sizeof( short )
         && addr % sizeof( short ) == 0 ) {

       *(short*)addr = hexbuf_to_long( sizeof( short ) * 2, hargs );
       hargs += sizeof( short ) * 2;
       addr += sizeof( short );
       len -= sizeof( short );
     }

     if( (len == 1) || ( len && ((addr % sizeof( short )) != 0) ) ) {

       /* request is totally misaligned;
          write a byte, and hope for the best */

       *(char*)addr = hexbuf_to_long( sizeof( char ) * 2, hargs );
       hargs += sizeof( char ) * 2;
       addr += sizeof( char );
       len -= sizeof( char );
     }
  }

  gdb_putmsg( 0, "OK", 2 );
  return;
}


/*
 */
void gdb_console_output( short len, const char *buf )
{
  char tx_buf[2];
  unsigned char sum;


  gdb_putc( '$' );
  sum = gdb_putstr( 1, "O" );

  while( len-- ) {
    tx_buf[0] = nibble_to_hex( *buf >> 4 );
    tx_buf[1] = nibble_to_hex( *buf++ );

    sum += gdb_putstr( 2, tx_buf );
  }

  /* send the message footer */
  gdb_putc( '#' );
  gdb_putc( nibble_to_hex( sum >> 4 ));
  gdb_putc( nibble_to_hex( sum ));

  /* DON'T wait for response; we don't want to get hung
     up here and halt the application if gdb has gone away! */
  
  return;
}


/*
 */
void gdb_write_registers ( char *hargs )
{
  short id = 0;
  long val;
  short reglen;


  while( *hargs != '#' ) {

    /* how big is this register? */
    reglen = gdb_peek_register_file( id, &val );

    if( reglen ) {

      /* extract the register's value */
      val = hexbuf_to_long( reglen * 2, hargs );
      hargs += sizeof( long ) * 2;

      /* stuff it into the register file */
      gdb_poke_register_file( id++, val );
    }

    else break;
  }

  gdb_putmsg( 0, "OK", 2 );

  return;
}


/*
 */
void gdb_read_registers ( char *hargs )
{
  char tx_buf[sizeof( long ) * 2];
  long val;
  short id = 0;
  short reglen;
  unsigned char sum;


  do {

    gdb_putc( '$' );
    sum = 0;

    /* send register values */
    while(( reglen = gdb_peek_register_file( id++, &val ) ) != 0 )
      sum += gdb_putstr( long_to_hexbuf( val, tx_buf, reglen * 2 ), tx_buf );

    /* send the message footer */
    gdb_putc( '#' );
    gdb_putc( nibble_to_hex( sum >> 4 ));
    gdb_putc( nibble_to_hex( sum ));

  } while( '+' != gdb_getc() );
    
  return;  
}


/*
 */
void gdb_write_register ( char *hargs )
{
  long id = 0;
  long val = 0;


  while( *hargs != '=' )
    id = ( id << 4 ) + hex_to_long( *hargs++ );

  hargs++;

  while( *hargs != '#' )
    val = ( val << 4 ) + hex_to_long( *hargs++ );

  gdb_poke_register_file( id, val );

  gdb_putmsg( 0, "OK", 2 );

  return;
}


/*
 * The gdb command processor.
 */
void gdb_monitor ( short sigval )
{
  char rxbuf[GDB_RXBUFLEN];
  char *hargs=rxbuf;
  unsigned long addr=0, type =-1, length =0;


  while( 1 ) {

    gdb_getmsg( rxbuf );

    hargs = rxbuf;
    switch( *hargs++ ) {
      
    case '?' : /* send last signal */

      gdb_last_signal( sigval );
      break;


    case 'c' : /* continue (address optional) */

		/* parse address, if any */
      while( *hargs != '#' )
	   {
        addr = ( addr << 4 ) + hex_to_long( *hargs++ );
	   }

      gdb_continue( addr );
      
      /* exit back to interrupted code */
      return;
      

    case 'g' :

      gdb_read_registers( hargs );
      break;


    case 'G' :

      gdb_write_registers( hargs );
      break;


    case 'H' :
      
      /* set thread---
	 unimplemented, but gdb wants it */
      gdb_putmsg( 0, "OK", 2 );
      break;


    case 'k' : /* kill program */

      gdb_putmsg( 0, "OK", 2 );
      gdb_kill();
      break;
     

    case 'm' :
      
      gdb_read_memory( hargs );
      break;


    case 'M' :

      gdb_write_memory( hargs );
      break;

      
    case 'P':

      gdb_write_register( hargs );
      break;


    case 'q' : /* query */

      /* TODO: finish query command in gdb_handle_exception. */

      /* for now, only respond to "unrecognized query" */
      gdb_putmsg( 0, "''", 2 );

      break;


    case 's' : /* step (address optional) */

		/* parse address, if any */
      while( *hargs != '#' )
	   {
        addr = ( addr << 4 ) + hex_to_long( *hargs++ );
	   }

      gdb_step( addr );
      
      /* exit back to interrupted code */
      return;

    case 'Z' : /* Insert break or watch-point:  Format:  Zt,addr,length  */
      type = hex_to_long( *hargs++ );
      if !(type >= 0 && type <=4){
	  gdb_putmsg(0,"E03",3);
	  return; }

      while ( *hargs++ != ',');	/* skip ',' */

      /* parse addr field */
      while ( *hargs != ',' )
	 addr = (addr << 4) + hex_to_long( *hargs++ );

      hargs++; /* skip second ',' */

      while (! *hargs)
	 length = (length << 4) + hex_to_long( *hargs++ );

      gdb_insert_watch(type, addr, length);  
      break;
      
    case 'z' : /* remove break or watch-point:  Format:  zt,addr,length  */
      type = hex_to_long( *hargs++ );
      if !(type >= 0 && type <=4){
          gdb_putmsg(0,"E04",3);
          return; }
 
      while ( *hargs++ != ','); /* skip ',' */
 
      /* parse addr field */
      while ( *hargs != ',' )
         addr = (addr << 4) + hex_to_long( *hargs++ );
 
      hargs++; /* skip second ',' */
 
      while (! *hargs)
         length = (length << 4) + hex_to_long( *hargs++ );
 
      gdb_remove_watch(type, addr, length);
      break; 

    default :
      
      /* received a command we don't recognize---
	 send empty response per gdb spec */
      gdb_putmsg( 0, "", 0 );
    
    }
  }

  return;
}


/*
 */
void gdb_handle_exception( long sigval )
{
  /* tell the host why we're here */
  gdb_expedited( sigval );

  /* ask gdb what to do next */
  gdb_monitor( sigval );

  /* return to the interrupted code */
  gdb_return_from_exception();

  return;
}

/********** Below is BlackFin DSP specific definition ***********

/*
 * Generic code to save processor context.
 */
#define SAVE_CONTEXT()				\
asm("						\
	[--sp] = (r7:7, p5:5);\n		\
	p5.l = CPUregisters;\n			\
	p5.h = CPUregisters;\n			\
	[p5++] = r0;\n				\
	[p5++] = r1;\n				\
	[p5++] = r2;\n				\
	[p5++] = r3;\n				\
	[p5++] = r4;\n				\
	[p5++] = r5;\n				\
	[p5++] = r6;\n				\
	[p5++] = r7;\n				\
	[p5++] = p1;\n				\
	[p5++] = p2;\n				\
	[p5++] = p3;\n				\
	[p5++] = p4;\n				\
	r7 = [sp];	/* get original p5 */ \n\
	[p5++] = r7;\n				\
	r7 = sp;\n				\
	r7 += -8;	/* get original sp */ \n\
	[p5++] = r7;\n				\
	[p5++] = fp;\n				\
	r7 = usp;\n				\
	[p5++] = r7;\n				\
					      \n\
	r7 = i0;\n				\
	[p5++] = r7;\n				\
	r7 = i1;\n				\
	[p5++] = r7;\n				\
	r7 = i2;\n				\
	[p5++] = r7;\n				\
	r7 = i3;\n				\
	[p5++] = r7;\n				\
					      \n\
	r7 = m0;\n				\
	[p5++] = r7;\n				\
	r7 = m1;\n				\
	[p5++] = r7;\n				\
	r7 = m2;\n				\
	[p5++] = r7;\n				\
	r7 = m3;\n				\
	[p5++] = r7;\n				\
					      \n\
	r7 = l0;\n				\
	[p5++] = r7;\n				\
	r7 = l1;\n				\
	[p5++] = r7;\n				\
	r7 = l2;\n				\
	[p5++] = r7;\n				\
	r7 = l3;\n				\
	[p5++] = r7;\n				\
					      \n\	
	r7 = b0;\n				\
	[p5++] = r7;\n				\
	r7 = b1;\n				\
	[p5++] = r7;\n				\
	r7 = b2;\n				\
	[p5++] = r7;\n				\
	r7 = b3;\n				\
	[p5++] = r7;\n				\
					      \n\
	r7 = a0x;\n				\
	[p5++] = r7;\n				\
	r7 = a0w;\n				\
	[p5++] = r7;\n				\
	r7 = a1x;\n				\
	[p5++] = r7;\n				\
	r7 = a1w;\n				\
	[p5++] = r7;\n				\
					      \n\
	r7 = ASTAT;\n				\
	[p5++] = r7;\n				\
	r7 = RETS;\n				\
	[p5++] = r7;\n				\
	r7 = RETI;\n				\
	[p5++] = r7;\n				\
	r7 = RETX;\n				\
	[p5++] = r7;	/* get breakpoint from here, former pc */\n\
	r7 = RETN;\n				\
	[p5++] = r7;\n				\
	r7 = RETE;\n				\
	[p5++] = r7;\n				\
	r7 = LC0;\n				\
	[p5++] = r7;\n				\
	r7 = LC1;\n				\
	[p5++] = r7;\n				\
	r7 = LT0;\n				\
	[p5++] = r7;\n				\
	r7 = LT1;\n				\
	[p5++] = r7;\n				\
	r7 = LB0;\n				\
	[p5++] = r7;\n				\
	r7 = LB1;\n				\
	[p5++] = r7;\n				\
	r7 = SYSCFG;\n				\
	[p5++] = r7;\n				\
	r7 = SEQSTAT;\n				\
	[p5++] = r7;\n				\
					      \n\
	(r7:7, p5:5) = [sp++];\n		\
");
/*	sp.l = localStackPtr;
 *
 *	sp.h = localStackPtr;	
 * switch to local stack - not currently working so commented out! 
 */

/*
 * Stuffs a unsigned char out the serial port.
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
 * Blocks until a unsigned char is received at the serial port.
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
 * Retrieves a register value from gdb_register_file.
 * Returns the size of the register, in bytes,
 * or zero if an invalid id is specified (which *will*
 * happen--- gdb.c uses this functionality to tell
 * how many registers we actually have).
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
 * Stuffs a register value into gdb_register_file.
 * Returns the size of the register, in bytes,
 * or zero if an invalid id is specified.
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
 * Uses the UBC to generate an exception
 * after we execute the next instruction.
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
 * Continue program execution at addr,
 * or at the current pc if addr == 0.
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
 * Kills the current application.
 * Simulates a reset by jumping to
 * the address taken from the reset
 * vector at address 0.
 */
void gdb_kill ( void )
{
}

/*
 * Flag for insn & data watchpoint MMR registers: 
 */
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
 *	EXCEPTION ISR (Single step/software breakpoint: EXCPT 15)
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
 * Restores registers to the values specified in CPUregisters
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

	BREAKPOINT();	/* Causes control to be passed to GDB stub */
}

int gdb_is_rom_addr(long addr)
{
	return (int)((addr >= 0x800000L) && (addr < 0xC00000L )); /* need more attention */
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


/************** Remote communication protocol *************
 
   A debug packet whose contents are <data>
   is encapsulated for transmission in the form:
 
        $ <data> # CSUM1 CSUM2
 
        <data> must be ASCII alphanumeric and cannot include characters
        '$' or '#'.  If <data> starts with two characters followed by
        ':', then the existing stubs interpret this as a sequence number.
 
        CSUM1 and CSUM2 are ascii hex representation of an 8-bit
        checksum of <data>, the most significant nibble is sent first.
        the hex digits 0-9,a-f are used.
 
   Receiver responds with:
 
        +       - if CSUM is correct and ready for next packet
        -       - if CSUM is incorrect
 
   <data> is as follows:
   All values are encoded in ascii hex digits.
 
        Request         Packet
 
        read registers  g
        reply           XX....X         Each byte of register data
                                        is described by two hex digits.
                                        Registers are in the internal order
                                        for GDB, and the bytes in a register
                                        are in the same order the machine uses.
                        or ENN          for an error.
 
        write regs      GXX..XX         Each byte of register data 
                                        is described by two hex digits.
        reply           OK              for success
                        ENN             for an error
 
        write reg       Pn...=r...      Write register n... with value r...,
                                        which contains two hex digits for each
                                        byte in the register (target byte
                                        order).
        reply           OK              for success
                        ENN             for an error
        (not supported by all stubs).
 
        read mem        mAA..AA,LLLL    AA..AA is address, LLLL is length.
        reply           XX..XX          XX..XX is mem contents
                                        Can be fewer bytes than requested
                                        if able to read only part of the data.
                        or ENN          NN is errno
 
        write mem       MAA..AA,LLLL:XX..XX
                                        AA..AA is address,
                                        LLLL is number of bytes,
                                        XX..XX is data
        reply           OK              for success
                        ENN             for an error (this includes the case
                                        where only part of the data was
                                        written).
 
        write mem       XAA..AA,LLLL:XX..XX
         (binary)                       AA..AA is address,
                                        LLLL is number of bytes,
                                        XX..XX is binary data
        reply           OK              for success
                        ENN             for an error
 
        cont            cAA..AA         AA..AA is address to resume
                                        If AA..AA is omitted,
                                        resume at same address.
 
        last signal     ?               Reply the current reason for stopping.
                                        This is the same reply as is generated
                                        for step or cont : SAA where AA is the
                                        signal number.
 
        There is no immediate reply to step or cont.
        The reply comes when the machine stops.
        It is           SAA             AA is the "signal number"
 
        or...           TAAn...:r...;n:r...;n...:r...;
                                        AA = signal number
                                        n... = register number
                                        r... = register contents
        or...           WAA             The process exited, and AA is
                                        the exit status.  This is only
                                        applicable for certains sorts of
                                        targets.
        kill request    k
 
        toggle debug    d               toggle debug flag (see 386 & 68k stubs)
        reset           r               reset -- see sparc stub.
        reserved        <other>         On other requests, the stub should
                                        ignore the request and send an empty
                                        response ($#<checksum>).  This way
                                        we can extend the protocol and GDB
                                        can tell whether the stub it is
                                        talking to uses the old or the new.
        search          tAA:PP,MM       Search backwards starting at address
                                        AA for a match with pattern PP and
                                        mask MM.  PP and MM are 4 bytes.
                                        Not supported by all stubs. 
 
        general query   qXXXX           Request info about XXXX.
        general set     QXXXX=yyyy      Set value of XXXX to yyyy.
        query sect offs qOffsets        Get section offsets.  Reply is
                                        Text=xxx;Data=yyy;Bss=zzz
        console output  Otext           Send text to stdout.  Only comes from
                                        remote target.
 
        Responses can be run-length encoded to save space.  A '*' means that
        the next character is an ASCII encoding giving a repeat count which
        stands for that many repititions of the character preceding the '*'.
        The encoding is n+29, yielding a printable character where n >=3
        (which is where rle starts to win).  Don't use an n > 126.
 
        So
        "0* " means the same as "0000".  
**************************************************************************/  
