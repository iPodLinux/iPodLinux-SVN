/*****************************************************************************/

/*
 *	lcdtxt.c -- driver for a text based LCD displays
 *
 *	(C) Copyright 2000-2002, Greg Ungerer (gerg@snapgear.com)
 *	(C) Copyright 2000-2001, Lineo Inc. (www.lineo.com) 
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <asm/param.h>
#include <asm/uaccess.h>

/*****************************************************************************/

/*
 *	Define driver major number.
 */
#define	LCDTXT_MAJOR	120

int		lcdtxt_line;
int		lcdtxt_pos;

#if LINUX_VERSION_CODE < 0x020100
#define GET_USER(a,b)   a = get_user(b)
#else
#include <asm/uaccess.h>
#define GET_USER(a,b)   get_user(a,b)
#endif

/*****************************************************************************/

/*
 *	Define normal text LCD control commands.
 */
#define	LCDTXT_CLEAR		0x01		/* Clear all of LCD display */
#define	LCDTXT_RETURN		0x02		/* Cursor to start of line */

#define	LCDTXT_MODE		0x04		/* Mode command */
#define	LCDTXT_MODE_INCR	0x02		/* Increment cursor on data */
#define	LCDTXT_MODE_DECR	0x00		/* Decrement cursor on data */
#define	LCDTXT_MODE_SHIFT	0x01		/* Display shift when full */
#define	LCDTXT_MODE_NOSHIFT	0x00		/* No display shift */

#define	LCDTXT_DISP		0x08		/* Display command */
#define	LCDTXT_DISP_ON		0x04		/* Turn display on */
#define	LCDTXT_DISP_OFF		0x00		/* Turn display off */
#define	LCDTXT_DISP_CUR_ON	0x02		/* Turn cursor on */
#define	LCDTXT_DISP_CUR_OFF	0x00		/* Turn cursor off */
#define	LCDTXT_DISP_BLNK_ON	0x01		/* Blinking cursor on */
#define	LCDTXT_DISP_BLNK_OFF	0x00		/* Blinking cursor off */

#define	LCDTXT_SHIFT		0x10		/* SHift command */
#define	LCDTXT_SHIFT_CUR	0x00		/* Cursor shift */
#define	LCDTXT_SHIFT_DISP	0x08		/* Display shift */
#define	LCDTXT_SHIFT_RIGHT	0x04		/* Right shift */
#define	LCDTXT_SHIFT_LEFT	0x00		/* Left shift */

#define	LCDTXT_FUNC		0x20		/* Function command */
#define	LCDTXT_FUNC_4BIT	0x00		/* 4 bit data bus */
#define	LCDTXT_FUNC_8BIT	0x10		/* 8 bit data bus */
#define	LCDTXT_FUNC_1LINE	0x00		/* 1 line display */
#define	LCDTXT_FUNC_2LINE	0x08		/* 2 line display */
#define	LCDTXT_FUNC_FONT	0x00		/* Standard font */

/*****************************************************************************/
#if defined(CONFIG_SECUREEDGEMP3)
/*****************************************************************************/

/*
 *	Hardware specifics for LCD on the LINEOMP3 board.
 *	This is a 16 character by 2 line device.
 */
#include <asm/coldfire.h>
#include <asm/mcfsim.h>

/* LCD display specifics */
#define	LCDTXT_LINENUM		2
#define	LCDTXT_LINELENGTH	16
#define LCDTXT_TABSTOP		8

/* LCD addressing info */
#define	LCDTXT_ADDR	0x30400000
static volatile unsigned char	*lcdp = (volatile unsigned char *) LCDTXT_ADDR;
static volatile unsigned short	*pp = (volatile unsigned short *)
					(MCF_MBAR + MCFSIM_PADAT);

/*
 *	LCD initial hardware setup.
 */
#define	lcdtxt_hwsetup()

/*
 *	LCD access functions.
 */
static void lcdtxt_writectrl(unsigned char ctrl)
{
	*pp &= ~0x0100;
	lcdp[0] = ctrl;
	lcdp[1] = 0;	/* Latch data */
	udelay(40);
}

static void lcdtxt_writedata(unsigned char val)
{
	*pp |= 0x0100;
	lcdp[0] = val;
	lcdp[1] = 0;	/* Latch data */
	udelay(40);
}

/*****************************************************************************/
#elif defined(CONFIG_eLIA)
/*****************************************************************************/

/*
 *	Hardware specifics for LCD on the eLIA board.
 *	This is a 2 line by 16 character LCD display device.
 */
#include <asm/coldfire.h>
#include <asm/mcfsim.h>

/* LCD display specifics */
#define	LCDTXT_LINENUM		2
#define	LCDTXT_LINELENGTH	16

/* LCD addressing info */
#define	LCDTXT_ADDR	0x30800000
static volatile unsigned char	*lcdp = (volatile unsigned char *) LCDTXT_ADDR;

/*
 *	LCD initial hardware setup.
 */
static void lcdtxt_hwsetiup(void)
{
	/* Setup CS4 for our external hardware */
	*((volatile unsigned short *) (MCF_MBAR + MCFSIM_CSAR4)) = 0x3080;
	*((volatile unsigned long *) (MCF_MBAR + MCFSIM_CSMR4)) = 0x000f0001;
	*((volatile unsigned short *) (MCF_MBAR + MCFSIM_CSCR4)) = 0x3d40;
	udelay(50000);
}

/*
 *	LCD access functions.
 */
static void lcdtxt_writectrl(unsigend char ctrl)
{
	lcdp[0] = ctrl;
	udelay(5000);
}

static unsigned char lcdtxt_readctrl(void)
{
	return(lcdp[0]);
}

static void lcdtxt_writedata(unsigend char val)
{
	lcdp[1] = val;
	udelay(2000);
}

/*****************************************************************************/
#else /* UNKNOWN HARDWARE */
/*****************************************************************************/

#error "LCDTXT: I don't know what hardware address to use?"

#endif
/*****************************************************************************/

int lcdtxt_open(struct inode *inode, struct file *filp)
{
#if LCDTXT_DEBUG
	printk("lcdtxt_open()\n");
#endif
	return(0);
}

/*****************************************************************************/

int lcdtxt_release(struct inode *inode, struct file *filp)
{
#if LCDTXT_DEBUG
	printk("lcdtxt_close()\n");
#endif
	return 0;
}

/*****************************************************************************/

void lcdtxt_resetposn(void)
{
	lcdtxt_writectrl(0x80 | (lcdtxt_line ? 0x40 : 0x00) | lcdtxt_pos);
}

/*****************************************************************************/

/* 
 * scroll buffer size
 */
#define SCROLL_SIZE	60

static struct timer_list scroll_timer;

/*
 *	Local copy of command settings of LCD.
 */
unsigned char	lcdtxt_disp;
unsigned char	lcdtxt_mode;
unsigned char 	scroll_buff[LCDTXT_LINENUM][SCROLL_SIZE];

int scroll_pos = 0;
int scroll_dir = 1;
int scroll_buff_line = 1;
int scrolling = 0;

/*****************************************************************************/

/*
 *	Magic super scroll function.
 */

static void scroll_text(void)
{
	int i, j;
	int larger, smaller;

	del_timer(&scroll_timer);
		
	i = strlen(scroll_buff[0]);
	larger = strlen(scroll_buff[1]);
	if (i > larger) {
		smaller = larger;
		larger = i;
	} else {
		smaller = i;
	}

	if (scroll_dir == 1) {
		if ((scroll_pos + LCDTXT_LINELENGTH) >= larger) {
			scroll_dir = -1;
		}
	} else {
		if (scroll_pos <= 0) {
			scroll_dir = 1;
		}
	}

	scroll_pos += scroll_dir;
	if (scroll_pos < 0)
		scroll_pos = 0;

	for (j = 0; j < LCDTXT_LINENUM ; j++) {
		if (strlen(scroll_buff[j]+scroll_pos) < LCDTXT_LINELENGTH) {
				lcdtxt_pos = strlen(scroll_buff[j]);
				lcdtxt_line = j;
				lcdtxt_resetposn();
				continue;
		} else {
			lcdtxt_line = j;
			lcdtxt_pos = 0;
			lcdtxt_resetposn();
		}
		for (i = scroll_pos; i < (scroll_pos + LCDTXT_LINELENGTH); i++) { 
			if (scroll_buff[j][i] != 0) {
				lcdtxt_writedata(scroll_buff[j][i]);
				lcdtxt_pos++;
			}
		}
	}
	if (larger <= LCDTXT_LINELENGTH) {
#if LCDTXT_DEBUG
		printk("lcdtxt: disabling autoscroll\n");
#endif
		del_timer(&scroll_timer);
	} else {
		scroll_timer.expires = jiffies + HZ;
		add_timer(&scroll_timer);
	}
}

/*****************************************************************************/

void lcdtxt_ceol(void)
{
	int		i;

#if LCDTXT_DEBUG
	printk("lcdtxt_ceol()\n");
#endif

	for (i = lcdtxt_pos; i < LCDTXT_LINELENGTH; i++)
		lcdtxt_writedata(' ');
}

/*****************************************************************************/

ssize_t lcdtxt_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	char	*dp, c;
	int	num;

	static unsigned char	 prog_need;
	static unsigned char	 setpos;

#if LCDTXT_DEBUG
	printk("lcdtxt_write(buf=%x,count=%d)\n", (int) buf, count);
#endif

	del_timer(&scroll_timer);
	dp = (char *) buf;

	for (num = 0; (num < count); num++) {
		GET_USER(c, dp++);

		/* If we're programming a character skip normal processing */
		if (prog_need != 0) {
			lcdtxt_writedata(c & 0x3f);
			if (--prog_need == 0)
				lcdtxt_resetposn();
			continue;
		}

		/*
		 * Now are we setting the cursor position directly.
		 * This code is a subset of the ioctl(fd, 3, ...) code
		 * below.  It can be included inline in a character stream
		 * but addresses a smaller range of rows and columns.
		 */
		if (setpos) {
			lcdtxt_pos = c & 0xf;
			lcdtxt_line = (c >> 4) & 0xf;
			setpos = 0;
			lcdtxt_resetposn();
			continue;
		}
		
		/* Normal character */
		switch ((unsigned char)c) {
		case 0x1:	/* CTRL-A  -- toggle cursor "on/off" */
			lcdtxt_disp ^= LCDTXT_DISP_CUR_ON;
			lcdtxt_writectrl(lcdtxt_disp);
			break;
		case 0x2:	/* CTRL-B  -- toggle blinking curser */
			lcdtxt_disp ^= LCDTXT_DISP_BLNK_ON;
			lcdtxt_writectrl(lcdtxt_disp);
			break;
		case 0x3:	/* CTRL-C  -- home cursor no clear */
			lcdtxt_writectrl(LCDTXT_RETURN);
			lcdtxt_pos = 0;
			lcdtxt_line = 0;
			udelay(1560);		// The rest of the 1600 usec delay
			break;
		case 0x5:	/* CTRL-E  -- clear to end of line */
			lcdtxt_ceol();
			memset(scroll_buff[lcdtxt_line], '\0', SCROLL_SIZE);
			lcdtxt_resetposn();
			break;
		case 0x6:	/* CTRL-F  -- advance curser 1 position */
			lcdtxt_pos++;
			lcdtxt_resetposn();
			break;
		case '\b':	/* CTRL-H -- step cursor back 1 position */
			lcdtxt_pos--;
			if(lcdtxt_pos < 0)
				lcdtxt_pos = 0;
			lcdtxt_resetposn();
			break;
		case '\t':	/* CTRL-I  -- tab to tabstop */
			do {
				lcdtxt_writedata(' ');
				lcdtxt_pos++;
			} while (lcdtxt_pos % LCDTXT_TABSTOP);
			break;

		case '\n':	/* CTRL-J  -- new line (no scroll) */
			lcdtxt_ceol();
			lcdtxt_pos = 0;
		case '\v':	/* CTRL-K  -- vertical tab (change lines) */
			lcdtxt_line = lcdtxt_line ? 0 : 1;
			memset(scroll_buff[lcdtxt_line], '\0', SCROLL_SIZE);
			lcdtxt_resetposn();
			break;

		case '\f':	/* CTRL-L  -- clear and home */
			lcdtxt_writectrl(LCDTXT_CLEAR);
			memset(scroll_buff[0], '\0', SCROLL_SIZE);
			memset(scroll_buff[1], '\0', SCROLL_SIZE);
			lcdtxt_line = 0;
			lcdtxt_pos = 0;
			scroll_pos = 0;
			scroll_dir = 1;
			udelay(1600);		// The rest of the 1640 usec delay
			break;
		case '\r':	/* CTRL-M  -- start of current line */
			lcdtxt_pos = 0;
			lcdtxt_resetposn();
			break;
		case 0x12:	/* CTRL-R  -- reset cursor: no blink, off */
			lcdtxt_disp &= ~(LCDTXT_DISP_BLNK_ON | LCDTXT_DISP_CUR_ON);
			lcdtxt_writectrl(lcdtxt_disp);
			break;
		case 0x1f:	/* ??????  -- set cursor x, y position */
			setpos = 1;
			break;
		case 0x80 ... 0x8f:	/* CG RAM character */
			lcdtxt_writedata(c & 0xf);
			lcdtxt_pos++;
			break;
		case 0x90 ... 0x97:	/* Program CG RAM character */
			prog_need = 8;
			lcdtxt_writectrl(0x40 + (c & 7) * 8);
			break;
		default:
			scroll_buff[lcdtxt_line][lcdtxt_pos] = c;
			if (lcdtxt_pos <= LCDTXT_LINELENGTH)
				lcdtxt_writedata(c);
			lcdtxt_pos++;
			break;
		}
	}
	if ((scroll_buff[0][LCDTXT_LINELENGTH] || scroll_buff[1][LCDTXT_LINELENGTH])
	    && scrolling == 1) {
#if LCDTXT_DEBUG
		printk("lcdtxt: engaging autoscroll\n");
#endif
		scroll_timer.expires = jiffies + HZ/2;
		add_timer(&scroll_timer);
	}

	return(num);
}

/*****************************************************************************/

int lcdtxt_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;

	switch (cmd) {

	case 1: /* turn scrolling on */
#if LCDTXT_DEBUG
		printk(KERN_INFO "lcdtxt:  scrolling enabled\n");
#endif
		scrolling = 1;
		scroll_timer.expires = jiffies + HZ/2;
		scroll_timer.function = (void *)scroll_text;
		add_timer(&scroll_timer);
		break;

	case 2: /* turn scrolling off */
#if LCDTXT_DEBUG
		printk(KERN_INFO "lcdtxt:  scrolling disabled\n");
#endif
		scrolling = 0;
		scroll_timer.function = NULL;
		del_timer(&scroll_timer);
		break;

	case 3: /* Directly address X/Y cursor position */
		/*
		 * This ioctl is essentailly a duplication of the
		 * 0x1f escape code code in the write routine.
		 * This version allows a wider range of values to be set
		 */
		lcdtxt_pos = arg & 0xff;
		lcdtxt_line = (arg >> 8) & 0xff;
		lcdtxt_resetposn();
		break;
		
	default:
		rc = -EINVAL;
		break;
	}

	return(rc);
}

/*****************************************************************************/

/*
 *	Exported file operations structure for driver...
 */

struct file_operations	lcdtxt_fops = {
	write:		lcdtxt_write,	/* write */
	ioctl:		lcdtxt_ioctl,	/* ioctl */
	open:		lcdtxt_open,	/* open */
	release:	lcdtxt_release,	/* release */
};


/*****************************************************************************/

static int __init lcdtxt_init(void)
{
	int	rc;

	/* Register lcdtxt as character device */
	if ((rc = register_chrdev(LCDTXT_MAJOR, "lcdtxt", &lcdtxt_fops)) < 0) {
		printk(KERN_WARNING "LCDTXT: can't get major %d\n",
			LCDTXT_MAJOR);
		return (-EBUSY);
	}

	printk("LCDTXT: Copyright (C) 2000-2001, Greg Ungerer "
		"(gerg@snapgear.com)\n");

	/* Hardware specific initialization */
	lcdtxt_hwsetup();

	/* Software copy of some commands */
	lcdtxt_disp = LCDTXT_DISP | LCDTXT_DISP_ON;
	lcdtxt_mode = LCDTXT_MODE | LCDTXT_MODE_INCR /*| LCDTXT_MODE_SHIFT*/;

	/*
	 *	Initialize the LCD controller.
	 *	Some data sheets recommend doing the init write twice...
	 */
	lcdtxt_writectrl(LCDTXT_FUNC | LCDTXT_FUNC_8BIT | LCDTXT_FUNC_2LINE);
	lcdtxt_writectrl(LCDTXT_FUNC | LCDTXT_FUNC_8BIT | LCDTXT_FUNC_2LINE);
	lcdtxt_writectrl(lcdtxt_disp);
	lcdtxt_writectrl(lcdtxt_mode);

	/* Set initial character position and clear display */
	lcdtxt_line = 0;
	lcdtxt_pos = 0;
	lcdtxt_writectrl(LCDTXT_CLEAR);

	/* default is scrolling on */
	scrolling = 1;
	scroll_timer.expires = jiffies + HZ/2;
	scroll_timer.function = (void *)scroll_text;
	add_timer(&scroll_timer);

	return 0;
}

module_init(lcdtxt_init);

/*****************************************************************************/
