#ifndef _armemu_h_
#define _armemu_h_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long ulong;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

#define  R_usr(x)  (x)
#define  FP        11
#define  IP        12
#define  SP        13
#define  LR        14
#define  PC        15
#define  R_fiq(x)  (((x) >= 8) && ((x) < 15)? 16+((x)-8) : (x))
#define  R8_fiq    16
#define  R9_fiq    17
#define  R10_fiq   18
#define  R11_fiq   19
#define  R12_fiq   20
#define  R13_fiq   21
#define  R14_fiq   22
#define  R_svc(x)  (((x) == 13) || ((x) == 14)? 23+((x)-13) : (x))
#define  R13_svc   23
#define  R14_svc   24
#define  R_abt(x)  (((x) == 13) || ((x) == 14)? 25+((x)-13) : (x))
#define  R13_abt   25
#define  R14_abt   26
#define  R_irq(x)  (((x) == 13) || ((x) == 14)? 27+((x)-13) : (x))
#define  R13_irq   27
#define  R14_irq   28
#define  R_und(x)  (((x) == 13) || ((x) == 14)? 29+((x)-13) : (x))
#define  R13_und   29
#define  R14_und   30

#define  CPSR      31
#define  SPSR_fiq  32
#define  SPSR_svc  33
#define  SPSR_abt  34
#define  SPSR_irq  35
#define  SPSR_und  36

#define  CPSR_control   0xff
#define  CPSR_mode      0x1f
#define  CPSR_mode_usr  0x10
#define  CPSR_mode_fiq  0x11
#define  CPSR_mode_irq  0x12
#define  CPSR_mode_svc  0x13
#define  CPSR_mode_abt  0x17
#define  CPSR_mode_und  0x1b
#define  CPSR_mode_sys  0x1f
#define  CPSR_T         0x20
#define  CPSR_F         0x40
#define  CPSR_I         0x80
#define  CPSR_flags     0xf0000000
#define  CPSR_V         0x10000000
#define  CPSR_C         0x20000000
#define  CPSR_Z         0x40000000
#define  CPSR_N         0x80000000
#define  CPSR_Vbits     28
#define  CPSR_Cbits     29
#define  CPSR_Zbits     30
#define  CPSR_Nbits     31

#define  IVEC_reset     0x00
#define  IVEC_und       0x04
#define  IVEC_swi       0x08
#define  IVEC_pabt      0x0C
#define  IVEC_dabt      0x10
#define  IVEC_irq       0x18
#define  IVEC_fiq       0x1C

typedef struct cpu
{
    u32 r[37];
} cpu_t;

typedef struct machine
{
    cpu_t *cpu;

    int iramsize;
    u32 irambase;
    void *iram;
    int dramsize;
    u32 drambase;
    void *dram;
    int flashsize;
    void *flash;

    int hw_ver;
    char pp_ver[8];

    int quickie;
    int verbose;
} machine_t;

void fatal (const char *err);

/* Magic to get the current SPSR */
#define SPSR -1
int reg (cpu_t *cpu, int r);

void fill_pipeline (cpu_t *cpu);
void reset (cpu_t *cpu);
u32 mapaddr (machine_t *mach, u32 vaddr);
int is_mmio (machine_t *mach, u32 paddr);
void *resolve_addr (machine_t *mach, u32 vaddr);
void interrupt (cpu_t *cpu, int vec, int mode);
u32 ldwi(machine_t *mach, cpu_t *cpu, u32 vaddr);
u32 ldw (machine_t *mach, cpu_t *cpu, u32 vaddr);
u16 ldh (machine_t *mach, cpu_t *cpu, u32 vaddr);
u8  ldb (machine_t *mach, cpu_t *cpu, u32 vaddr);
void stw (machine_t *mach, cpu_t *cpu, u32 vaddr, u32 value);
void sth (machine_t *mach, cpu_t *cpu, u32 vaddr, u16 value);
void stb (machine_t *mach, cpu_t *cpu, u32 vaddr, u8  value);
int cond (cpu_t *cpu, int ccode);
u32 barrel_shift (cpu_t *cpu, u32 shdesc, int *cflagp);
void execute (machine_t *mach, cpu_t *cpu);
char *disassemble (u32 instr, u32 addr);
void update_screen (machine_t *mach);
void disassemble_image(const char *filename);

#endif
