#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <linux/init.h>


#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/spi.h> 

#if !defined(SEEK_SET)
#define SEEK_SET 0
#endif


static unsigned int   spi_major = 60; /* a local major, can be overwritten */
static          int   openflag  =  0;
static np_spi * const spi_ptr   = na_spi;

                    /*******************************/
                    /* SPI data transfer routines. */
                    /*******************************/

#define SPI_XMIT_READY np_spistatus_trdy_mask
#define SPI_RECV_READY np_spistatus_rrdy_mask

#define SPI_BUSYPOLL_TIMEOUT 1000

// returns -1 if there is no data present, otherwise returns
// the value
inline int SPI_Recv_Byte(char *pdata )
{
	if (spi_ptr->np_spistatus & SPI_RECV_READY){
		*pdata = spi_ptr->np_spirxdata & 0xff;
		return 0;
	}
	return  -1;
}


// Sends the 16 bit address+data
inline int SPI_Send_Byte( unsigned char address, char data )
{
	u16 value = ((address & 0xFF) << 8) | (data & 0xFF);

	if ( spi_ptr->np_spistatus & SPI_XMIT_READY ) {
		spi_ptr->np_spitxdata = value;
		return 0;
	}
	
	return -1;
}



                    /*************************/
                    /* SPI Driver functions. */
                    /*************************/

int spi_reset( void )
{
  // Nothing to do: The Nios does _not_
  // support burst xfers. Chip Enables
  // (Selects) are inactive after each xfer.
  return 0;
}


/***************************************************/
/* The SPI Write routine. The first 16 bits are     */
/* the device register, and the rest of the buffer */
/* is data.                                        */
/***************************************************/

ssize_t spi_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
  int            i;
  unsigned char addr;
  int timeout;
  char temp;
  
  if ( count < 3 )
    return -EINVAL;          	       /* Address is 2 bytes: Must have _something_ to send */

  addr = buf[0];          /* chip register address.  */
  spi_ptr->np_spistatus=0;

  for ( i = sizeof(u16); i<count; i++ ) 
  {
  	timeout=SPI_BUSYPOLL_TIMEOUT;
  	while (SPI_Send_Byte(addr, buf[i])==-1) 
  	{
		if (--timeout==0) 
		{
		  printk("spi_write time out\n");
		  return i; /* return the number of bytes sent */
		}
  	}
  	/* read the data */
  	timeout=SPI_BUSYPOLL_TIMEOUT;
 	while (SPI_Recv_Byte(&temp)==-1) 
 	{
		if (--timeout==0) 
		  break; 
  	}
  }
  return i; 
//  unsigned char *temp;                 /* Our pointer to the buffer */
//  int            i;
//  int            addr;
//  
//  if ( count < 3 )
//    return -EINVAL;          	       /* Address is 2 bytes: Must have _something_ to send */
//
//  temp = (char *)buf;
//  addr = (int)*((u16 *)temp);          /* chip register address.  */
//  temp += sizeof(u16);
//
//  for ( i = count - sizeof(u16); i; i--, temp++ )
//      *temp = (unsigned char)SPI_Transfer( addr, (int)*temp );
//    
//  
//  return count;                        /* we can always send all data */
}

//int spi_read( struct inode *inode, struct file *file, char *buf, int count )
ssize_t spi_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
  int            i;
  unsigned char addr;
  int timeout;
  char temp;
  
  if ( count < 3 )
    return -EINVAL;          	       /* Address is 2 bytes: Must have _something_ to send */

  addr = buf[0];          /* chip register address.  */
  spi_ptr->np_spistatus=0;
  
  /* empty the np_spirxdata register */
  SPI_Recv_Byte(&temp);
  
  for ( i = sizeof(u16); i<count; i++ ) 
  {
  	/* send the address */
  	timeout=SPI_BUSYPOLL_TIMEOUT;
  	while (SPI_Send_Byte(addr, 0)==-1) 
  	{
		if (--timeout==0) 
		{
		  printk("spi_read write address time out\n");
		  return i; 
		}
  	}
  	
  	/* read the data */
  	timeout=SPI_BUSYPOLL_TIMEOUT;
 	while (SPI_Recv_Byte(&buf[i])==-1) 
 	{
		if (--timeout==0) 
		{
		  printk("spi_read read data time out\n");
		  return i; 
		}
  	}
#if 0
  	printk("spi_read time left %d\n", timeout);
#endif
  }
  return i;
}

loff_t spi_lseek(struct file *filp, loff_t offset, int origin)
{
#if 0
  int     bit_count, i;
#endif

  if ( origin != SEEK_SET || offset != (offset & 0xFFFF) )
  {
      errno = -EINVAL;
      return -1;
  }

#if 0
  /****************************************/
  /* Nios SPI implementation safeguard:   */
  /* It is possible to have more than     */
  /* one CS active at a time. Check that  */
  /* the given address is a power of two. */
  /****************************************/
  bit_count = 0;
  for ( i = 0; i < sizeof(u16); i++ )
  {
      if ( (1 << i) & offset )
      {
	  if ( ++bit_count > 1 )
	  {
	      errno = -EINVAL;
	      return -1;
	  }
      }
  }
#endif
  spi_ptr->np_spislaveselect = offset;
  return 0;
}

int spi_open(struct inode *inode, struct file *filp)
{
  if ( openflag )
    return -EBUSY;

  MOD_INC_USE_COUNT;
  openflag = 1;

  return 0;
}

int spi_release(struct inode *inode, struct file *filp)
{
  openflag = 0;
  MOD_DEC_USE_COUNT;
	return 0;
}


/* static struct file_operations spi_fops  */

static struct file_operations spi_fops = {
	llseek:		spi_lseek,     /* Set chip-select line. The offset is used as an address. */
	read:		spi_read,
	write:		spi_write,
	open:		spi_open, 
	release:	spi_release,
};


int register_NIOS_SPI( void )
{
  int result = register_chrdev( spi_major, "spi", &spi_fops );
  if ( result < 0 )
  {
    printk( "SPI: unable to get major %d for SPI bus \n", spi_major );
    return result;
  }/*end-if*/
  
  if ( spi_major == 0 )
    spi_major = result; /* here we got our major dynamically */

  /* reserve our port, but check first if free */
  if ( check_region( (unsigned int)na_spi, sizeof(np_spi) ) )
  {
    printk( "SPI: port at adr 0x%08x already occupied\n", (unsigned int)na_spi );
    unregister_chrdev( spi_major, "spi" );

    return result;
  }/*end-if*/

  return 0;
}

void unregister_NIOS_SPI( void )
{
    if ( spi_major > 0 )
      unregister_chrdev( spi_major, "spi" );

    release_region( (unsigned int)na_spi, sizeof(np_spi) );
}


#ifdef MODULE
void cleanup_module( void )
{
  unregister_NIOS_SPI();
}



int init_module( void )
{
  return register_NIOS_SPI();
}
#endif


static int __init nios_spi_init(void)
{
	printk("SPI: Nios SPI bus device version 0.1\n");
	return register_NIOS_SPI();
//	if ( register_NIOS_SPI() )
//		printk("*** Cannot initialize SPI device.\n");
}

__initcall(nios_spi_init);
