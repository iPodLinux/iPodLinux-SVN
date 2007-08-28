/*
 * Low-level parallel-port routines for Hitachi HD64465 hardware.
 *
 * Author: Tomoyoshi ASANO <asa@lineo.com>
 *
 * based on work by:
 *          Eddie C. Dost <ecd@skynet.be>
 *          Phil Blundell <Philip.Blundell@pobox.com>
 *          Tim Waugh <tim@cyberelk.demon.co.uk>
 *         Jose Renau <renau@acm.org>
 *          David Campbell <campbell@tirian.che.curtin.edu.au>
 *          Grant Guenther <grant@torque.net>
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/init.h>

#include <linux/parport.h>

#include <asm/io.h>
#include <asm/irq.h>
#ifdef CONFIG_SH_KEYWEST
#include <asm/hitachi_keywest.h>
#endif
#ifdef CONFIG_SH_HD64465
#include <asm/hd64465.h>
#endif

#define DATA     0x00
#define STATUS   0x02
#define CONTROL  0x04
#define EPPADDR  0x06
#define EPPDATA  0x08
#define CFIFO    0x10
#define DFIFO    0x10
#define TFIFO    0x10
#define CONFIGA  0x10
#define CONFIGB  0x12
#define ECONTROL 0x14

static void
parport_hd64465_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  parport_generic_irq(irq, (struct parport *) dev_id, regs);
}

void
parport_hd64465_write_epp(struct parport *p, unsigned char d)
{
  outb(d, p->base + EPPDATA);
}

unsigned char
parport_hd64465_read_epp(struct parport *p)
{
  return inb(p->base + EPPDATA);
}

void
parport_hd64465_write_epp_addr(struct parport *p, unsigned char d)
{
  outb(d, p->base + EPPADDR);
}

unsigned char
parport_hd64465_read_epp_addr(struct parport *p)
{
 return inb(p->base + EPPADDR);
}

int parport_hd64465_epp_clear_timeout(struct parport *pb);

int
parport_hd64465_check_epp_timeout(struct parport *p)
{
  if (!(inb(p->base+STATUS) & 1))
    return 0;
  parport_hd64465_epp_clear_timeout(p);
  return 1;
}

unsigned char
parport_hd64465_read_configb(struct parport *p)
{
  return inb(p->base + CONFIGB);
}

void
parport_hd64465_write_data(struct parport *p, unsigned char d)
{
  outb(d, p->base + DATA);
}

unsigned char
parport_hd64465_read_data(struct parport *p)
{
  return inb(p->base + DATA);
}

void
parport_hd64465_write_control(struct parport *p, unsigned char d)
{
  outb(d, p->base + CONTROL);
}

unsigned char
parport_hd64465_read_control(struct parport *p)
{
  return inb(p->base + CONTROL);
}

unsigned char
parport_hd64465_frob_control(struct parport *p, unsigned char mask, unsigned char val)
{
  unsigned char old = inb(p->base + CONTROL);
  outb(((old & ~mask) ^ val), p->base + CONTROL);
  return old;
}

void
parport_hd64465_write_status(struct parport *p, unsigned char d)
{
  outb(d, p->base + STATUS);
}

unsigned char
parport_hd64465_read_status(struct parport *p)
{
  return inb(p->base + STATUS);
}

void
parport_hd64465_write_econtrol(struct parport *p, unsigned char d)
{
  outb(d, p->base + ECONTROL);
}

unsigned char
parport_hd64465_read_econtrol(struct parport *p)
{
  return inb(p->base + ECONTROL);
}

unsigned char
parport_hd64465_frob_econtrol(struct parport *p, unsigned char mask, unsigned char val)
{
  unsigned char old = inb(p->base + ECONTROL);
  outb(((old & ~mask) ^ val), p->base + ECONTROL);
  return old;
}

void
parport_hd64465_change_mode(struct parport *p, int m)
{
  parport_hd64465_frob_econtrol(p, 0xe0, m << 5);
}

void
parport_hd64465_write_fifo(struct parport *p, unsigned char v)
{
  outb(v, p->base + DFIFO);
}

unsigned char
parport_hd64465_read_fifo(struct parport *p)
{
  return inb(p->base + DFIFO);
}

void
parport_hd64465_disable_irq(struct parport *p)
{
}

void
parport_hd64465_enable_irq(struct parport *p)
{
}

void
parport_hd64465_release_resources(struct parport *p)
{
  if (p->irq != PARPORT_IRQ_NONE) {
    parport_hd64465_disable_irq(p);
    free_irq(p->irq, p);
  }
}

int
parport_hd64465_claim_resources(struct parport *p)
{
  int err;
  if (p->irq != PARPORT_IRQ_NONE) {
    if ((err = request_irq(p->irq, parport_hd64465_interrupt,
                          0, p->name, p)) != 0)
      return err;
    else
      parport_hd64465_enable_irq(p);
  }
  return 0;
}

void
parport_hd64465_init_state(struct parport_state *s)
{
  s->u.pc.ctr = 0xc;
  s->u.pc.ecr = 0x0;
}

void
parport_hd64465_save_state(struct parport *p, struct parport_state *s)
{
  s->u.pc.ctr = parport_hd64465_read_control(p);
  s->u.pc.ecr = parport_hd64465_read_econtrol(p);
}

void
parport_hd64465_restore_state(struct parport *p, struct parport_state *s)
{
  parport_hd64465_write_control(p, s->u.pc.ctr);
  parport_hd64465_write_econtrol(p, s->u.pc.ecr);
}

size_t
parport_hd64465_epp_read_block(struct parport *p, void *buf, size_t length)
{
  return 0;
}

size_t
parport_hd64465_epp_write_block(struct parport *p, void *buf, size_t length)
{
  return 0;
}

int
parport_hd64465_ecp_read_block(struct parport *p, void *buf, size_t length,
                              void (*fn)(struct parport *, void *, size_t),
                              void *handle)
{
  return 0;
}

int
parport_hd64465_ecp_write_block(struct parport *p, void *buf, size_t length,
                               void (*fn)(struct parport *, void *, size_t),
                               void *handle)
{
  return 0;
}

void
parport_hd64465_inc_use_count(void)
{
#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif
}

void
parport_hd64465_dec_use_count(void)
{
#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif
}

static void parport_hd64465_fill_inode(struct inode *inode, int fill)
{
#ifdef MODULE
  if (fill)
    MOD_INC_USE_COUNT;
  else
    MOD_DEC_USE_COUNT;
#endif
}
#if 0
static struct parport_operations parport_hd64465_ops =
{
  parport_hd64465_write_data,
  parport_hd64465_read_data,

  parport_hd64465_write_control,
  parport_hd64465_read_control,
  parport_hd64465_frob_control,

  parport_hd64465_write_econtrol,
  parport_hd64465_read_econtrol,
  parport_hd64465_frob_econtrol,

  parport_hd64465_write_status,
  parport_hd64465_read_status,

  parport_hd64465_write_fifo,
  parport_hd64465_read_fifo,

  parport_hd64465_change_mode,

  parport_hd64465_release_resources,
  parport_hd64465_claim_resources,

  parport_hd64465_write_epp,
  parport_hd64465_read_epp,
  parport_hd64465_write_epp_addr,
  parport_hd64465_read_epp_addr,
  parport_hd64465_check_epp_timeout,

  parport_hd64465_epp_write_block,
  parport_hd64465_epp_read_block,

  parport_hd64465_ecp_write_block,
  parport_hd64465_ecp_read_block,

  parport_hd64465_init_state,
  parport_hd64465_save_state,
  parport_hd64465_restore_state,

  parport_hd64465_enable_irq,
  parport_hd64465_disable_irq,
  parport_hd64465_interrupt,

  parport_hd64465_inc_use_count,
  parport_hd64465_dec_use_count,
  parport_hd64465_fill_inode
};
#endif
static struct parport_operations parport_hd64465_ops =
{
  parport_hd64465_write_data,
  parport_hd64465_read_data,

  parport_hd64465_write_control,
  parport_hd64465_read_control,
  parport_hd64465_frob_control,

  //parport_hd64465_write_econtrol,
  //parport_hd64465_read_econtrol,
  //parport_hd64465_frob_econtrol,

  parport_hd64465_read_status,

  parport_hd64465_enable_irq,
  parport_hd64465_disable_irq,

          //parport_pc_data_forward,
	  //parport_pc_data_reverse,
  
  parport_hd64465_init_state,
  parport_hd64465_save_state,
  parport_hd64465_restore_state,

  parport_hd64465_inc_use_count,
  parport_hd64465_dec_use_count,
/* kasu
  parport_hd64465_write_epp,
  parport_hd64465_read_epp,
  parport_hd64465_write_epp_addr,
  parport_hd64465_read_epp_addr,
*/ 
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

 /* 
  parport_hd64465_write_fifo,
  parport_hd64465_read_fifo,

  parport_hd64465_change_mode,

  parport_hd64465_release_resources,
  parport_hd64465_claim_resources,

  parport_hd64465_check_epp_timeout,

  parport_hd64465_epp_write_block,
  parport_hd64465_epp_read_block,

  parport_hd64465_ecp_write_block,
  parport_hd64465_ecp_read_block,


  parport_hd64465_interrupt,

  parport_hd64465_fill_inode

  parport_hd64465_write_status,
  */
};


/*
 * Clear TIMEOUT BIT in EPP MODE
 */
int parport_hd64465_epp_clear_timeout(struct parport *pb)
{
  unsigned char r;

  if (!(parport_hd64465_read_status(pb) & 0x01))
    return 1;

  /* To clear timeout some chips require double read */
  parport_hd64465_read_status(pb);
  r = parport_hd64465_read_status(pb);
  parport_hd64465_write_status(pb, r | 0x01); /* Some reset by writing 1 */
  parport_hd64465_write_status(pb, r & 0xfe); /* Others by writing 0 */
  r = parport_hd64465_read_status(pb);

  return !(r & 0x01);
}

/* Check for ECP
 *
 * Old style XT ports alias io ports every 0x400, hence accessing ECONTROL
 * on these cards actually accesses the CTR.
 *
 * Modern cards don't do this but reading from ECONTROL will return 0xff
 * regardless of what is written here if the card does NOT support
 * ECP.
 *
 * We will write 0x2c to ECONTROL and 0xcc to CTR since both of these
 * values are "safe" on the CTR since bits 6-7 of CTR are unused.
 */
static int parport_ECR_present(struct parport *pb)
{
  unsigned int r;
  unsigned char octr = pb->ops->read_control(pb),
    oecr = pb->ops->read_econtrol(pb);

  r = pb->ops->read_control(pb);
  if ((pb->ops->read_econtrol(pb) & 0x3) == (r & 0x3)) {
    pb->ops->write_control(pb, r ^ 0x2 ); /* Toggle bit 1 */

    r = pb->ops->read_control(pb);
    if ((pb->ops->read_econtrol(pb) & 0x2) == (r & 0x2)) {
      pb->ops->write_control(pb, octr);
      return 0; /* Sure that no ECONTROL register exists */
    }
  }

  if ((pb->ops->read_econtrol(pb) & 0x3 ) != 0x1)
    return 0;

  pb->ops->write_econtrol(pb, 0x34);
  if (pb->ops->read_econtrol(pb) != 0x35)
    return 0;

  pb->ops->write_econtrol(pb, oecr);
  pb->ops->write_control(pb, octr);

/* yagi modified next 2 lines */
/*  return PARPORT_MODE_PCECR; */
  return PARPORT_MODE_DMA;
}

static int parport_ECP_supported(struct parport *pb)
{
  int i;
  unsigned char oecr = pb->ops->read_econtrol(pb);

  /* If there is no ECONTROL, we have no hope of supporting ECP. */
/* yagi modified next 2 lines */
/*  if (!(pb->modes & PARPORT_MODE_PCECR)) */
  if (!(pb->modes & PARPORT_MODE_DMA))
    return 0;

  /*
   * Using LGS chipset it uses ECONTROL register, but
   * it doesn't support ECP or FIFO MODE
   */

  pb->ops->write_econtrol(pb, 0xc0); /* TEST FIFO */
  for (i=0; i < 1024 && (pb->ops->read_econtrol(pb) & 0x01); i++)
    pb->ops->write_fifo(pb, 0xaa);

  pb->ops->write_econtrol(pb, oecr);
/* yagi modified next 2 lines */
/*  return (i == 1024) ? 0 : PARPORT_MODE_PCECP; */
  return (i == 1024) ? 0 : PARPORT_MODE_ECP;
}

static int parport_PS2_supported(struct parport *pb)
{
  int ok = 0;
  unsigned char octr = pb->ops->read_control(pb);

  pb->ops->write_control(pb, octr | 0x20);

  pb->ops->write_data(pb, 0x55);
  if (pb->ops->read_data(pb) != 0x55) ok++;

  pb->ops->write_data(pb, 0xaa);
  if (pb->ops->read_data(pb) != 0xaa) ok++;

  pb->ops->write_control(pb, octr);

/* yagi modified next 2 lines */
/*  return ok ? PARPORT_MODE_PCPS2 : 0; */
  return ok ? PARPORT_MODE_TRISTATE : 0;
}

static int parport_ECPPS2_supported(struct parport *pb)
{
  int mode;
  unsigned char oecr = pb->ops->read_econtrol(pb);

/* yagi modified next 2 lines */
/*  if (!(pb->modes & PARPORT_MODE_PCECR)) */
  if (!(pb->modes & PARPORT_MODE_DMA))
    return 0;

  pb->ops->write_econtrol(pb, 0x20);

  mode = parport_PS2_supported(pb);

  pb->ops->write_econtrol(pb, oecr);

/* yagi modified next 2 lines */
/*  return mode ? PARPORT_MODE_PCECPPS2 : 0; */
  return mode ? PARPORT_MODE_SAFEININT : 0;
}

/* yagi modified next 14 lines */
/* #define printmode(x) \
{ \
  if (p->modes & PARPORT_MODE_PC##x) { \
    printk("%s%s", f ? "," : "", #x); \
    f++; \
  } \
} */
#define printmode(x) \
{ \
  if (p->modes & PARPORT_MODE_##x) { \
    printk("%s%s", f ? "," : "", #x); \
    f++; \
  } \
}

/* yagi added next 1 lines */
void (*parport_probe_hook)(struct parport *port);

int
init_one_port(void)
{
  struct parport *p;
  unsigned long base;
  int irq, dma;

/* yagi modified next 4 lines */
/*  base = HD64465_IOBASE + 0xA000; */
  base = CONFIG_HD64465_IOBASE + 0xA000;
/*  irq  = HD64465_IRQBASE + 3; */
  irq  = HD64465_IRQ_BASE + 3;
  dma  = PARPORT_DMA_NONE;

  if (!(p = parport_register_port(base, irq, dma, &parport_hd64465_ops)))
    return 0;

  p->modes = PARPORT_MODE_PCSPP | parport_PS2_supported(p);
  p->modes |= parport_ECR_present(p);
  p->modes |= parport_ECP_supported(p);
  p->modes |= parport_ECPPS2_supported(p);
  p->size = 3;

  p->dma = PARPORT_DMA_NONE;

  printk(KERN_INFO "%s: PC-style at 0x%lx", p->name, p->base);
  if (p->irq != PARPORT_IRQ_NONE)
    printk(", irq %d", p->irq);
  printk(" [");
  {
    int f = 0;
/* yagi modified next 8 lines */
/*    printmode(SPP);
    printmode(PS2);
    printmode(ECP);
    printmode(ECPPS2); */
    printmode(PCSPP);
    printmode(TRISTATE);
    printmode(ECP);
    printmode(SAFEININT);
  }
  printk("]\n");
  parport_proc_register(p);

/* yagi added next 1 line */
  printk("yagi***init_one_port:1");
  
  p->flags |= PARPORT_FLAG_COMA;

  p->ops->write_control(p, 0x0c);
  p->ops->write_data(p, 0);

  if (parport_probe_hook)
    (*parport_probe_hook)(p);

  return 1;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE
int init_module(void)
#else
/* yagi modified next 2 lines */
/* __initfunc(int parport_hd64465_init(void)) */
int __init parport_hd64465_init(void)
#endif
{
  int count = 0;
  count += init_one_port();

/* yagi added next 1 line */
  printk("yagi***parport_hd64465_init:init_one_port()=%d, count=%d\n",init_one_port(),count);

  return count ? 0 : -ENODEV;
}

#ifdef MODULE
void
cleanup_module(void)
{
  struct parport *p = parport_enumerate(), *tmp;
  while (p) {
    tmp = p->next;
    if (p->modes & PARPORT_MODE_PCSPP) {
      if (!(p->flags & PARPORT_FLAG_COMA))
       parport_quiesce(p);
      parport_proc_unregister(p);
      parport_unregister_port(p);
    }
    p = tmp;
  }
}
#endif
