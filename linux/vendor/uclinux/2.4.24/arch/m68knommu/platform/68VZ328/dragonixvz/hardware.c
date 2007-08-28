/*
 *  linux/arch/m68knommu/platform/68VZ328/dragonixvz/hardware.c
 *
 *  (C)  2002 by      Daniel Haensse <daniel.haensse@alumni.ethz.ch>
 *  Use it under GPL
 */

/* Common functions */

#include <asm/MC68VZ328.h>




void putC (char ch)
{
	UTX_TXDATA = ch;
	while (!(UTX & 0x2000));
}

void putS (char * str)
{
	while (*str) putC (*(str++));
}
void printhex (unsigned long n)
{
unsigned long	i;
	if ((i = n / 16)) printhex (i);
	if (n % 16 > 9) putC (n % 16 + '7');
	else putC (n % 16 + '0');
}


void uwait(unsigned long x)
{
 int loop;
 while(x>0)
 {
  for(loop=0;loop<10;loop++);
  x--;
 }
}

/* SPI functions */

#define NONE 0
#define PEN  1
#define CLK  2


unsigned long SPIIO(unsigned char chip, unsigned long shifts, unsigned long out)
{
 int loop;
 unsigned long in=0;

 *(volatile unsigned  char *)0x4000103=(chip&0x03);
 uwait(5);

 for(loop=shifts;loop>0;loop--)
 {
  *(volatile unsigned  char *)0x4000103= (((out&(1<<(loop-1)))!=0 ? 0x10 : 0x00) | (chip&0x03));        /* data out */
  uwait(1);
  *(volatile unsigned  char *)0x4000103= (((out&(1<<(loop-1)))!=0 ? 0x10 : 0x00) | (chip&0x03) | 0x04); /* clk */
  uwait(5);
  in=(in<<1) | (((*(volatile unsigned  char *)0x4000103)&0x10)>>4);
  uwait(5);
  *(volatile unsigned  char *)0x4000103= (((out&(1<<(loop-1)))!=0 ? 0x10 : 0x00) | (chip&0x03));        /* data out */
  uwait(5);
 }
 *(volatile unsigned  char *)0x4000103=0; /* deselect all */
 return(in);
}


/* I2C Functions */

unsigned char I2CStart()
{
 *(volatile unsigned  char *)0x4000102= 0x0f; /* SDA=1 , SCL =1 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0b; /* SDA=0 , SCL =1 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0a; /* SDA=0 , SCL =0 */
 uwait(5);
}

void I2CACK()
{

 *(volatile unsigned  char *)0x4000102= 0x0b; /* SDA=0 , SCL =1 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0a; /* SDA=0 , SCL =0 */
 uwait(5);
}

void I2CNACK()
{
 *(volatile unsigned  char *)0x4000102= 0x0f; /* SDA=1 , SCL =1 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0e; /* SDA=1 , SCL =0 */
 uwait(5);
}


unsigned char I2CSendByte(unsigned char data)
{
 int loop;
 for(loop=0;loop<8;loop++)
 {
  if(data&0x80)
  {
   *(volatile unsigned  char *)0x4000102= 0x0e; /* SDA=1 , SCL =0 */
   uwait(1);
   *(volatile unsigned  char *)0x4000102= 0x0f; /* SDA=1 , SCL =1 */
   uwait(5);
   *(volatile unsigned  char *)0x4000102= 0x0e; /* SDA=1 , SCL =0 */
   uwait(5);

  }
  else
  {
   *(volatile unsigned  char *)0x4000102= 0x0a; /* SDA=0 , SCL =0 */
   uwait(1);
   *(volatile unsigned  char *)0x4000102= 0x0b; /* SDA=0 , SCL =1 */
   uwait(5);
   *(volatile unsigned  char *)0x4000102= 0x0a; /* SDA=0 , SCL =0 */
   if(loop!=7)
   {
    uwait(5);
   }
  }
  data=data<<1;
 }
 *(volatile unsigned  char *)0x4000102= 0x02; /* SDA=Z , SCL =0 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x03; /* SDA=Z , SCL =1 */
 uwait(5);
 loop=((*(volatile unsigned  char *)0x4000102) & 0x04)>>2;
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0e; /* SDA=1 , SCL =0 */
 uwait(5);
}

unsigned char I2CGetByte()
{
 int loop;
 unsigned char data=0;
 for(loop=0;loop<8;loop++)
 {
  *(volatile unsigned  char *)0x4000102= 0x02; /* SDA=Z , SCL =0 */
  uwait(5);
  *(volatile unsigned  char *)0x4000102= 0x03; /* SDA=Z , SCL =1 */
  uwait(5);
  data|=((*(volatile unsigned  char *)0x4000102) & 0x04)<<5;
  uwait(5);
  *(volatile unsigned  char *)0x4000102= 0x02; /* SDA=Z , SCL =0 */
 }
 *(volatile unsigned  char *)0x4000102= 0x0a; /* SDA=0 , SCL =0 */
 uwait(5);
 return(data);
}

unsigned char I2CStop()
{
 *(volatile unsigned  char *)0x4000102= 0x0a; /* SDA=0 , SCL =0 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0b; /* SDA=0 , SCL =1 */
 uwait(5);
 *(volatile unsigned  char *)0x4000102= 0x0f; /* SDA=1 , SCL =1 */
 uwait(5);
}

/* Abstract functions, please keep the functions untouched when we improve the functionality of the FPGA */

unsigned char BCD_TO_BIN(unsigned char val)
{
        return( ((val)&15) + ((val)>>4)*10 );
}

/* Read Real Time Clock Location */
unsigned char RTCRead(unsigned int addr)
{
 return( SPIIO(CLK,16,addr<<8) &0xff );
}

/* Write Real Time Clock Location */
void RTCWrite(unsigned int addr,unsigned char data)
{
 SPIIO(CLK,16,(addr<<8)|0x8000 | data);
}


/* Set Contrast Voltage, see pdf for table */
void SetContrast(unsigned char data)
{
 I2CStart();                                          /* Send start bit */
 I2CSendByte(0x58);
 I2CSendByte(data);
 I2CStop();
}

/* Backlight */
void BacklightOn()
{
 *(volatile unsigned  char *) 0xfffff409 &=~0x80;
}

void BacklightOff()
{
 *(volatile unsigned  char *) 0xfffff409 |=0x80;
}

/* We use the following mapping of the cmos ram */
/*
        0x20 xor crc of memory 0x21 to 0x7F
        0x21 lcdcontrast
        0x22,0x23,0x24,0x25,0x26,0x27 mac address 1
        0x28,0x29,0x2a,0x2b,0x2c,0x2d mac address 2
        0x2e and following are unused
*/

unsigned char rtccmosinvalid()
{
 unsigned char crccalc=0,crc;
 unsigned int addr;
 crc=RTCRead(0x20);
 for(addr=0x21;addr<0x80;addr++)  crccalc^=RTCRead(addr);
 if(crc!=crccalc)
 {
 /* Folks if you use a display that needs a voltage,
    change this otherwise it will be destroyed  */
        RTCWrite(0x0f,0x00);    /* Remove write protection */
        RTCWrite(0x21,0x00);    /* Switch contrast off */
        RTCWrite(0x22,0x00);    /* Mac 1 */
        RTCWrite(0x23,0x08);
        RTCWrite(0x24,0x9D);
        RTCWrite(0x25,0x00);
        RTCWrite(0x26,0x00);
        RTCWrite(0x27,0x00);
        RTCWrite(0x28,0x00);    /* Mac 2 */
        RTCWrite(0x29,0x08);
        RTCWrite(0x2a,0x9D);
        RTCWrite(0x2b,0x00);
        RTCWrite(0x2c,0x00);
        RTCWrite(0x2d,0x01);
        crccalc=0;
        for(addr=0x21;addr<0x80;addr++)  crccalc^=RTCRead(addr);
        RTCWrite(0x20,crccalc); /* Revalidate CMOS, leave time unchanged */
        RTCWrite(0x0f,0x40);    /* Enable write protection */
        return(0x01);
 }
 else
        return(0x00);
}

/* Charge Lithium Battery */
void rtcchargebattery()
{
 RTCWrite(0x0f,0x00);    /* Remove write protection */
 RTCWrite(0x11,0xa5);    /* 3.9-0.7V = 3.2V Zenervoltage sourced over 200R resistor and 2k serie resistor */
 RTCWrite(0x0f,0x40);    /* Enable write protection */
}

void rtcgettod(int *yearp, int *monp, int *dayp, int *hourp, int *minp, int *secp)
{
	*secp   =       BCD_TO_BIN(RTCRead(0x00));
	*minp   =       BCD_TO_BIN(RTCRead(0x01));
	*hourp  =       BCD_TO_BIN(RTCRead(0x02));
	*dayp   =       BCD_TO_BIN(RTCRead(0x04));
	*monp   =       BCD_TO_BIN(RTCRead(0x05));
	*yearp  =       BCD_TO_BIN(RTCRead(0x06));
}

/* LCD contrast */
unsigned char lcdsetcontrast()
{
 unsigned char contr=RTCRead(0x21);
 SetContrast(contr);
 return(contr);
}

/* Get hardware address */
void gethwaddr(unsigned char dev, unsigned char *addr)
{
 unsigned char i;
 if(dev==0) // Mac 1
 {
  for(i=0;i<6;i++)
  {
   addr[i]=RTCRead(0x22+i);
  }
 }
 else // Mac 2
 {
  for(i=0;i<6;i++)
  {
   addr[i]=RTCRead(0x28+i);
  }
 }
}

/* Copy date from external to internal RTC */
void rtcgetintern()
{
 /* We must run the RTC in 24 hour mode!!! */
/* RTCTIME=BCD_TO_BIN(RTCRead(0x00))
        |(BCD_TO_BIN(RTCRead(0x01))<<16)
        |(BCD_TO_BIN(RTCRead(0x02))<<24);
  */

}

