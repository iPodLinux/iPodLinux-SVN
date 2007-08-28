/* Low-level parallel port routines for the Dragonix VZ add on board
 *
 * Author: Daniel Haensse <daniel.haensse@dragonix.net>
 *
 * Parallel port is emulated by the general purpose io pins
 *
 * DIRECTION    : PJ0
 *
 * nSTB (1)  -> : PK2
 * DB0  (2)  <->: PE0
 * DB1  (3)  <->: PE1
 * DB2  (4)  <->: PE2
 * DB3  (5)  <->: PE3
 * DB4  (6)  <->: PK4
 * DB5  (7)  <->: PK5
 * DB6  (8)  <->: PK6
 * DB7  (9)  <->: PK7
 * nACK (10) <- : PD4
 * BSY  (11) <- : PD5
 * PE   (12) <- : PD6
 * SEL  (13) <- : PF1
 * nAF  (14)  ->: PJ1
 * nER  (15) <- : PD2
 * nPI  (16)  ->: PJ2
 * nSEL (17)  ->: PJ3
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/ioport.h>
#include <asm/setup.h>
#include <asm-m68knommu/MC68VZ328.h>
#include <asm/irq.h>
#include <asm/io.h>


static struct parport *this_port = NULL;
static unsigned char ctr_copy;

static void dragonix_write_data(struct parport *p, unsigned char data)
{
	//printk(KERN_INFO  "write_data %c\n",data);
        PEDATA=(PEDATA&~0x0f)|(data&0x0f);
        PKDATA=(PKDATA&~0xf0)|(data&0xf0);
}

static unsigned char dragonix_read_data(struct parport *p)
{
	return( (PEDATA&0x0f) | (PKDATA&0xf0) );
}

static __inline__ unsigned char __dragonix_ioport_frob_control (struct parport *p,
							   unsigned char mask,
							   unsigned char val)
{
        unsigned long flags;
        unsigned char ctr = ctr_copy;
	ctr = (ctr & ~mask) ^ val;
        ctr_copy = ctr;	/* Update soft copy */

        save_flags(flags);
	cli();
        PJDATA=(PJDATA&~0x0e) | ((ctr_copy^0x0a)&0x0e); // invert nAF and nSEL, leave DIR untouched
        PKDATA=(PKDATA&~0x04) | ( (ctr_copy&0x01) ? 0x00 : 0x04 );

        if(ctr_copy&0x20)
        { // input
          PEDIR&=~0x0f;
          PKDIR&=~0xf0;
          PJDATA&=~0x01;
        }
        else
        { // output
          PJDATA|=0x01;
          PEDIR|=0x0f;
          PKDIR|=0xf0;
        }

        restore_flags(flags);

        if(ctr_copy&0x10)
        {
          enable_irq(IRQ2_IRQ_NUM);
        }
        else
        {
          disable_irq(IRQ2_IRQ_NUM);
        }

        return ctr;
}

static void dragonix_write_control(struct parport *p, unsigned char d)
{
	const unsigned char wm = (PARPORT_CONTROL_STROBE |
				  PARPORT_CONTROL_AUTOFD |
				  PARPORT_CONTROL_INIT |
				  PARPORT_CONTROL_SELECT);

	/* Take this out when drivers have adapted to the newer interface. */
	if (d & 0x20) {
		/*printk (KERN_INFO "%s (%s): use data_reverse for this!\n",
			p->name, p->cad->name);*/
		dragonix_data_reverse (p);
	}

	__dragonix_ioport_frob_control (p, wm, d & wm);
}

static unsigned char dragonix_read_control( struct parport *p)
{
       	const unsigned char wm = (PARPORT_CONTROL_STROBE |
				  PARPORT_CONTROL_AUTOFD |
				  PARPORT_CONTROL_INIT |
				  PARPORT_CONTROL_SELECT);
       	//printk(KERN_INFO  "read_control \n");
	return ctr_copy & wm; /* Use soft copy */
}

static unsigned char dragonix_frob_control( struct parport *p, unsigned char mask, unsigned char val)
{
        /*
        * DIRECTION    : PJ0, 1
        * nSTB (1)  -> : PK2, 1
        * nAF  (14)  ->: PJ1  0
        * nPI  (16)  ->: PJ2  0
        * nSEL (17)  -> : PJ3  1
        */
        	const unsigned char wm = (PARPORT_CONTROL_STROBE |
				  PARPORT_CONTROL_AUTOFD |
				  PARPORT_CONTROL_INIT |
				  PARPORT_CONTROL_SELECT);

	/* Take this out when drivers have adapted to the newer interface. */
	if (mask & 0x20) {
		/*printk (KERN_INFO "%s (%s): use data_%s for this!\n",
			p->name, p->cad->name,
			(val & 0x20) ? "reverse" : "forward");*/
		if (val & 0x20)
			dragonix_data_reverse (p);
		else
			dragonix_data_forward (p);
	}

	/* Restrict mask and val to control lines. */
	mask &= wm;
	val &= wm;

	return __dragonix_ioport_frob_control (p, mask, val);
}


static unsigned char dragonix_read_status(struct parport *p)
{
	unsigned char status;
        unsigned long flags;
        /*
        * nACK (10) <- : PD4, I
        * BSY  (11) <- : PD5  I
        * PE   (12) <- : PD6  I
        * SEL  (13) <- : PF1  I
        * nER  (15) <- : PD2  I
        */
       	save_flags(flags);
	cli();
        status=(((PDDATA<<2)&0x80) ^ 0x80 ) /*BSY*/
                | ((PFDATA<<3)&0x10) /* SEL */| ((PDDATA>>1)&0x20) /* PE */
                | ((PDDATA<<1)&0x08) /* nER */
                | (((PDDATA<<2)&0x40)^0x40) /* nACK */
                |0x07;
        restore_flags(flags);
	//printk(KERN_INFO  "read_status %02x\n", status);
	return status;
}

/* as this ports irq handling is already done, we use a generic funktion */
static void dragonix_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        //printk(KERN_INFO "irq\n");
        ISR|=ISR_IRQ2; // Clear IRQ2
        parport_generic_irq(irq, (struct parport *) dev_id, regs);
}

static void dragonix_enable_irq(struct parport *p)
{
        __dragonix_ioport_frob_control(p, 0x10, 0x10);
}

static void dragonix_disable_irq(struct parport *p)
{
        __dragonix_ioport_frob_control (p, 0x10, 0);
}

static void dragonix_data_forward(struct parport *p)
{
	//printk(KERN_INFO  "forward\n");
	__dragonix_ioport_frob_control (p, 0x20, 0); // output
}

static void dragonix_data_reverse(struct parport *p)
{
	//printk(KERN_INFO  "reverse\n");
        __dragonix_ioport_frob_control (p, 0x20, 0x20); // input
}

static void dragonix_init_state(struct pardevice *dev, struct parport_state *s)
{
        //printk(KERN_INFO "state\n");
        s->u.pc.ctr = 0xc | (dev->irq_func ? 0x10 : 0x00);
}

static void dragonix_save_state(struct parport *p, struct parport_state *s)
{
        s->u.pc.ctr=ctr_copy;
}

static void dragonix_restore_state(struct parport *p, struct parport_state *s)
{

        /*
        * DIRECTION    : PJ0, 1
        *
        * nSTB (1)  -> : PK2, 1
        * nAF  (14)  ->: PJ1  0
        * nPI  (16)  ->: PJ2  0
        * nSEL (17)  -> : PJ3  1
        */
        unsigned long flags;

        ctr_copy=s->u.pc.ctr;
	save_flags(flags);
	cli();
        PJDATA=(PJDATA&~0x0e) | ((ctr_copy^0x0a)&0x0e); // invert nAF and nSEL, leave DIR untouched
        PKDATA=(PKDATA&~0x04) | ( (ctr_copy&0x01) ? 0x00 : 0x04 );
        if(ctr_copy&0x20)
        { // input
          PEDIR&=~0x0f;
          PKDIR&=~0xf0;
          PJDATA&=~0x01;
        }
        else
        { // output
          PJDATA|=0x01;
          PEDIR|=0x0f;
          PKDIR|=0xf0;
        }
        restore_flags(flags);

        if(ctr_copy&0x10)
        {
          enable_irq(IRQ2_IRQ_NUM);
        }
        else
        {
          disable_irq(IRQ2_IRQ_NUM);
        }
}

static void dragonix_inc_use_count(void)
{
#ifdef MODULE
        MOD_INC_USE_COUNT;
#endif
}

static void dragonix_dec_use_count(void)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct parport_operations pp_dragonix_ops = {
	dragonix_write_data,
	dragonix_read_data,

	dragonix_write_control,
	dragonix_read_control,
	dragonix_frob_control,

	dragonix_read_status,

	dragonix_enable_irq,
	dragonix_disable_irq,

	dragonix_data_forward,
	dragonix_data_reverse,

	dragonix_init_state,
	dragonix_save_state,
	dragonix_restore_state,

	dragonix_inc_use_count,
	dragonix_dec_use_count,

	parport_ieee1284_epp_write_data,
	parport_ieee1284_epp_read_data,
	parport_ieee1284_epp_write_addr,
	parport_ieee1284_epp_read_addr,

	parport_ieee1284_ecp_write_data,
	parport_ieee1284_ecp_read_data,
	parport_ieee1284_ecp_write_addr,

	parport_ieee1284_write_compat,
	parport_ieee1284_read_nibble,
	parport_ieee1284_read_byte,
};

/* ----------- Initialisation code --------------------------------- */

int __init parport_dragonix_init(void)
{
	struct parport *p;
        int err;
        unsigned long flags;

        if(this_port!=NULL) return(-ENODEV);
        /* With the current implementation just initialize IO port*/
        /*
        * DIRECTION    : PJ0, 1
        *
        * nSTB (1)  -> : PK2, 1
        * DB0  (2)  <->: PE0, 0
        * DB1  (3)  <->: PE1, 0
        * DB2  (4)  <->: PE2, 0
        * DB3  (5)  <->: PE3, 0
        * DB4  (6)  <->: PK4, 0
        * DB5  (7)  <->: PK5, 0
        * DB6  (8)  <->: PK6, 0
        * DB7  (9)  <->: PK7, 0
        * nACK (10) <- : PD4, I
        * BSY  (11) <- : PD5  I
        * PE   (12) <- : PD6  I
        * SEL  (13)  <-: PF1  I
        * nAF  (14)  ->: PJ1  0
        * nER  (15) <- : PD2  I
        * nPI  (16)  ->: PJ2  0
        * nSEL (17)  -> : PJ3  1
        */

        /* PJ0=DIR, PJ1=nAF, PJ2=nPI, PJ3=nSEL */
        PJSEL|=0x0f;
        PJDATA=(PJDATA&~0x0f)|0x0b;
        PJDIR|=0x0f;
        PJPUEN&=~0x0f; // we don't need pull-ups

        /* PF1=SEL */
        PFSEL|=0x02;
        PFDIR&=~0x02;
        PFPUEN&=~0x02; // we don't need pull-ups

        /* PD2=nER PD4=nACK PD5=BSY PD6=PE */
        PDSEL|=0x74;
        PDDIR&=~0x74;
        PDPUEN&=~0x74; // we don't need pull-ups

        /* PK2=nSTB PK4=DB4 PK5=DB5 PK6=DB6 PK7=DB7 */
        PKSEL|=0xf4;
        PKDIR|=0xf4;
        PKDATA=(PKDATA&~0xf4)|0x04;
        PKPUEN&=~0xf4;

        /* PE0=DB0 PE1=DB1 PE2=DB2 PE3=DB3 */
        PESEL|=0x0f;
        PEDIR|=0x0f;
        PEDATA&=~0x0f;
        PEPUEN&=~0x0f;

	err = -EBUSY;

        p = parport_register_port(PADIR_ADDR, IRQ2_IRQ_NUM,PARPORT_DMA_NONE, &pp_dragonix_ops);
        p->modes = PARPORT_MODE_PCSPP|PARPORT_MODE_TRISTATE;
	p->size = 1;
	if (!p)
		goto out_port;
	err = request_irq(IRQ2_IRQ_NUM, &dragonix_interrupt, IRQ_FLG_STD, p->name, p);
        if (err)
		goto out_irq;



        /* Set irq port we go for falling edge of irq2
           a spurious int may be created */
	save_flags(flags);
	cli();
        ICR|=ICR_ET2;
        ICR&=~ICR_POL2;
        //*(volatile unsigned short *)0xfffff302 &= ~0x1100;
        ISR|=ISR_IRQ2; // Clear IRQ2
        PDSEL&=~0x20;
        restore_flags(flags);

	this_port = p;
	printk(KERN_INFO "%s: Dragonix VZ add-on parallel port using irq %d\n", p->name,IRQ2_IRQ_NUM);
	/* XXX: set operating mode */
	parport_proc_register(p);

	parport_announce_port(p);


	return 0;

out_irq:
	parport_unregister_port(p);
out_port:
	return err;
}

void __exit parport_dragonix_exit(void)
{
	if (this_port->irq != PARPORT_IRQ_NONE)
		free_irq(IRQ2_IRQ_NUM, this_port);
	parport_proc_unregister(this_port);
	parport_unregister_port(this_port);
}



//module_init(parport_dragonix_init)
//module_exit(parport_dragonix_exit)

#ifdef MODULE
MODULE_AUTHOR("Daniel Haensse <daniel.haensse@dragonix.net>");
MODULE_DESCRIPTION("Parport Driver for Dragonix Add-on Port");
MODULE_SUPPORTED_DEVICE("Dragonix Parallel Port");
MODULE_LICENSE("GPL");

int init_module(void)
{
 parport_dragonix_init()

}

void cleanup_module(void)
{
        parport_dragonix_exit();
}
#endif
