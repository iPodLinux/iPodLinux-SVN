#include "armemu.h"

void fatal (const char *err) 
{
    fprintf (stderr, "Fatal error. %s\n", err);
    exit (1);
}

int reg (cpu_t *cpu, int r) 
{
    if (r < 0) {
        switch (cpu->r[CPSR] & 0xF) {
        case 1: return SPSR_fiq;
        case 2: return SPSR_irq;
        case 3: return SPSR_svc;
        case 7: return SPSR_abt;
        case 11: return SPSR_und;
        default: return CPSR;
        }
    }

    switch (cpu->r[CPSR] & 0xF) {
        case 1: return R_fiq(r);
        case 2: return R_irq(r);
        case 3: return R_svc(r);
        case 7: return R_abt(r);
        case 11: return R_und(r);
        default: return R_usr(r);
    }
}

void fill_pipeline (cpu_t *cpu)
{
    cpu->r[PC] += 8;
}

void reset (cpu_t *cpu)
{
    cpu->r[R13_svc] = cpu->r[reg (cpu, SP)];
    cpu->r[R14_svc] = cpu->r[reg (cpu, LR)];
    cpu->r[SPSR_svc] = cpu->r[reg (cpu, CPSR)];
    cpu->r[PC] = cpu->r[LR] = 0;
    cpu->r[CPSR] = (cpu->r[CPSR] & ~(CPSR_mode | CPSR_flags | CPSR_T)) | CPSR_I | CPSR_F | CPSR_mode_svc;
}

u32 mapaddr (machine_t *mach, u32 vaddr) 
{
    // Page-mapping would go here.
    return vaddr;
}

int is_mmio (machine_t *mach, u32 paddr) 
{
    if (mach->hw_ver < 0x40000) {
        return (((paddr & 0xf0000000) == 0xc0000000) ||
                ((paddr & 0xf0000000) == 0xf0000000));
    } else {
        if (((paddr & 0xf0000000) == 0x30000000) && ((mach->hw_ver >> 16) == 0xB)) /* MIO */
            return 1;

        return (((paddr & 0xf0000000) == 0x60000000) || /* PSB */
                ((paddr & 0xf0000000) == 0x70000000) || /* APB */
                ((paddr & 0xc0000000) == 0xc0000000)); /* AHB and CCH */
    }
}

void *resolve_addr (machine_t *mach, u32 vaddr) 
{
    u32 paddr = mapaddr (mach, vaddr);
    
    if (is_mmio (mach, paddr)) {
        fatal ("resolve_addr() called on a MMIO address");
    }

    if ((paddr >= mach->irambase) && (paddr < mach->irambase + mach->iramsize)) {
        return (void *)((u8 *)mach->iram + (paddr - mach->irambase));
    } else if ((paddr >= mach->drambase) && (paddr < mach->drambase + mach->dramsize)) {
        return (void *)((u8 *)mach->dram + (paddr - mach->drambase));
    }
    return 0; // ->fault
}

// Since we don't actually have a pipeline, PC
// is always just 8 bytes ahead of the instruction
// that's currently being fetched, decoded, and
// executed.

void interrupt (cpu_t *cpu, int vec, int mode) 
{
    u32 spsr = cpu->r[CPSR];

    cpu->r[CPSR] = (cpu->r[CPSR] & ~(CPSR_mode | CPSR_T)) | mode;

    switch (vec) {
    case IVEC_reset:
        reset (cpu);
        break;
    case IVEC_und:
        cpu->r[SPSR_und] = spsr;
        cpu->r[R14_und] = cpu->r[PC] - 4;
        break;
    case IVEC_swi:
        cpu->r[SPSR_svc] = spsr;
        cpu->r[R14_svc] = cpu->r[PC] - 4;
        break;
    case IVEC_pabt:
        cpu->r[SPSR_abt] = spsr;
        cpu->r[R14_abt] = cpu->r[PC] - 4;
        break;
    case IVEC_dabt:
        cpu->r[SPSR_abt] = spsr;
        cpu->r[R14_abt] = cpu->r[PC];
        break;
    case IVEC_fiq:
        cpu->r[SPSR_fiq] = spsr;
        cpu->r[R14_fiq] = cpu->r[PC] - 4;
        cpu->r[CPSR] |= CPSR_F;
        break;
    case IVEC_irq:
        cpu->r[SPSR_irq] = spsr;
        cpu->r[R14_irq] = cpu->r[PC] - 4;
        cpu->r[CPSR] |= CPSR_I;
        break;
    default:
        fatal ("Unrecognized interrupt!");
    }

    cpu->r[PC] = vec;
    fill_pipeline (cpu);
}

u32 readPPver (machine_t *mach, u32 addr)
{
    return ((u32 *)mach->pp_ver)[(addr - 0x70000000) >> 2];
}

#define IPOD_UNK0    0x1
#define IPOD_1G      0x2
#define IPOD_2G      0x4
#define IPOD_3G      0x8
#define IPOD_MINI1G  0x10
#define IPOD_4G      0x20
#define IPOD_PHOTO   0x40
#define IPOD_MINI2G  0x80
#define IPOD_UNK8    0x100
#define IPOD_UNK9    0x200
#define IPOD_UNKA    0x400
#define IPOD_5G      0x800
#define IPOD_NANO    0x1000
#define IPOD_UNKD    0x2000
#define IPOD_UNKE    0x4000
#define IPOD_REV(r)   (((r)<<16)|0x8000)
#define IPOD_HASREV  0x8000
#define IPOD_GETREV(x) (((x)>>16)&0xffff)

#define IPOD_COLOR   IPOD_PHOTO
#define IPOD_MINI    IPOD_MINI1G|IPOD_MINI2G
#define IPOD_VIDEO   IPOD_5G

#define IPOD_PP5002  IPOD_1G|IPOD_2G|IPOD_3G
#define IPOD_PP5020  IPOD_4G|IPOD_MINI1G|IPOD_PHOTO
#define IPOD_PP5022  IPOD_MINI2G|IPOD_5G|IPOD_NANO
#define IPOD_PP502X  IPOD_PP5020|IPOD_PP5022

u32 debugRead (machine_t *mach, u32 addr) 
{
    u32 ret;
    printf (">>< Please enter a 32-bit hex value for a read from %08x: ", addr);
    scanf ("%x", &ret);
    return ret;
}

// The debug interface is:
// 0x6ffffff0 - write a 4-char constant
// 0x6ffffff4 - write a string address
// 0x6ffffff8 - write a delay
// 0x6ffffffc - write a number
void debugWrite (machine_t *mach, u32 addr, u32 value) 
{
    char *ptr;
    switch (addr) {
    case 0x6ffffff0:
        printf (">>> 4-char str: %c%c%c%c\n", value & 0xff,
                (value >> 8) & 0xff, (value >> 16) & 0xff, value >> 24);
        break;
    case 0x6ffffff4:
        ptr = resolve_addr (mach, value);
        if (memchr (ptr, '\0', 512))
            printf (">>> %s\n", ptr);
        else
            printf (">>> invalid or >512c string %08x output\n", value);
        break;
    case 0x6ffffff8:
        usleep (value);
        break;
    case 0x6ffffffc:
        printf (">>> Number: %d (%08x)\n", value, value);
        break;
    }
}

struct mmio {
    int pods;
    u32 addr;
    int elemsize; // 1 for byte reads, 2 for word, 4 for dword
    int totalsize; // number of [size] elements it covers
    u32 (*readfn)(machine_t *mach, u32 addr); // if 0, returns 0x55555555 on the CPU and 0xAAAAAAAA on the COP
    void (*writefn)(machine_t *mach, u32 addr, u32 value);
} mmios[] = {
    /* CPU/COP ID */
    { IPOD_PP5002, 0xc4000000, 4, 4, 0, 0 },
    { IPOD_PP502X, 0x60000000, 4, 4, 0, 0 },

    /* PP5020/5022 version */
    { IPOD_PP502X, 0x70000000, 4, 8, readPPver, 0 },

    /* Our debug stuffs */
    { IPOD_PP502X, 0x6ffffff0, 4, 16, debugRead, debugWrite },

    { 0, 0, 0, 0, 0, 0 },
};

#undef RELAXED_MMIOS

u32 mmio_read32 (machine_t *mach, cpu_t *cpu, u32 paddr) 
{
    struct mmio *mmiop = mmios;
    paddr &= ~3;
    while (mmiop->addr != 0) {
        if ((paddr >= mmiop->addr) && (paddr + 4 <= mmiop->addr + mmiop->totalsize)) {
            if (!mmiop->readfn)
                return 0x55555555;
            
            switch (mmiop->elemsize) {
            case 4:
                return mmiop->readfn (mach, paddr);
                break;
            case 2:
                return ((mmiop->readfn (mach, paddr) & 0xffff) |
                        ((mmiop->readfn (mach, paddr + 2) << 16) & 0xffff));
            case 1:
                return ((mmiop->readfn (mach, paddr) & 0xff) |
                        ((mmiop->readfn (mach, paddr + 1) << 8) & 0xff00) |
                        ((mmiop->readfn (mach, paddr + 2) << 16) & 0xff0000) |
                        ((mmiop->readfn (mach, paddr + 3) << 24) & 0xff000000));
                break;
            }
            return 0;
        }
        mmiop++;
    }

#ifdef RELAXED_MMIOS
    printf (">>! Read32 from unimplemented MMIO address %p! Returning 0.\n", paddr);
    return 0;
#endif
    fprintf (stderr, ">>! Read32 from unimplemented MMIO address %p. Aborting.\n", paddr);
    exit (1);
}

u16 mmio_read16 (machine_t *mach, cpu_t *cpu, u32 paddr) 
{
    struct mmio *mmiop = mmios;
    paddr &= ~1;
    while (mmiop->addr != 0) {
        if ((paddr >= mmiop->addr) && (paddr + 2 <= mmiop->addr + mmiop->totalsize)) {
            if (!mmiop->readfn)
                return 0x5555;
            
            switch (mmiop->elemsize) {
            case 4:
                // If the addr is aligned, we're getting the bottom hword
                // (least-significant).
                return (mmiop->readfn (mach, paddr & ~3) >> ((paddr & 2) << 3)) & 0xffff;
                break;
            case 2:
                return mmiop->readfn (mach, paddr) & 0xffff;
                break;
            case 1:
                return ((mmiop->readfn (mach, paddr) & 0xff) |
                        ((mmiop->readfn (mach, paddr + 1) << 8) & 0xff));
                break;
            }
            return 0;
        }
        mmiop++;
    }

#ifdef RELAXED_MMIOS
    printf (">>! Read16 from unimplemented MMIO address %p! Returning 0.\n", paddr);
    return 0;
#endif
    fprintf (stderr, ">>! Read16 from unimplemented MMIO address %p. Aborting.\n", paddr);
    exit (1);
}

u8 mmio_read8 (machine_t *mach, cpu_t *cpu, u32 paddr) 
{
    struct mmio *mmiop = mmios;
    while (mmiop->addr != 0) {
        if ((paddr >= mmiop->addr) && (paddr < mmiop->addr + mmiop->totalsize)) {
            if (!mmiop->readfn)
                return 0x55;
            
            switch (mmiop->elemsize) {
            case 4:
                return (mmiop->readfn (mach, paddr & ~3) >> ((paddr & 3) << 3)) & 0xff;
                break;
            case 2:
                return (mmiop->readfn (mach, paddr & ~1) >> ((paddr & 1) << 3)) & 0xff;
                break;
            case 1:
                return mmiop->readfn (mach, paddr) & 0xff;
                break;
            }
            return 0;
        }
        mmiop++;
    }

#ifdef RELAXED_MMIOS
    printf (">>! Read8 from unimplemented MMIO address %p! Returning 0.\n", paddr);
    return 0;
#endif
    fprintf (stderr, ">>! Read8 from unimplemented MMIO address %p! Returning 0.\n", paddr);
    exit (1);
}

void mmio_write32 (machine_t *mach, u32 paddr, u32 value) 
{
    struct mmio *mmiop = mmios;
    paddr &= ~3;
    while (mmiop->addr != 0) {
        if ((paddr >= mmiop->addr) && (paddr + 4 <= mmiop->addr + mmiop->totalsize)) {
            if (!mmiop->writefn)
                return;
            
            switch (mmiop->elemsize) {
            case 4:
                mmiop->writefn (mach, paddr, value);
                return;
            case 2:
                mmiop->writefn (mach, paddr, value & 0xffff);
                mmiop->writefn (mach, paddr + 2, (value >> 16) & 0xffff);
                return;
            case 1:
                mmiop->writefn (mach, paddr, value & 0xff);
                mmiop->writefn (mach, paddr + 1, (value >> 8) & 0xff);
                mmiop->writefn (mach, paddr + 2, (value >> 16) & 0xff);
                mmiop->writefn (mach, paddr + 3, (value >> 24) & 0xff);
                return;
            }
            return;
        }
        mmiop++;
    }

#ifdef RELAXED_MMIOS
    printf (">>! Write32 of 0x%08x to unimplemented MMIO address %p! Ignoring.\n", value, paddr);
#else
    fprintf (stderr, ">>! Write32 of 0x%08x to unimplemented MMIO address %p! Aborting.\n", value, paddr);
    exit (1);
#endif
}

void mmio_write16 (machine_t *mach, u32 paddr, u16 value)
{
    struct mmio *mmiop = mmios;
    paddr &= ~1;
    while (mmiop->addr != 0) {
        if ((paddr >= mmiop->addr) && (paddr + 2 <= mmiop->addr + mmiop->totalsize)) {
            if (!mmiop->writefn)
                return;
            
            switch (mmiop->elemsize) {
            case 4:
                if (!mmiop->readfn) return;
                u32 val32 = mmiop->readfn (mach, paddr & ~3);
                val32 &= ~(0xffff << ((paddr & 2) << 3));
                val32 |= value << ((paddr & 2) << 3);
                mmiop->writefn (mach, paddr & ~3, val32);
                return;
            case 2:
                mmiop->writefn (mach, paddr, value);
                return;
            case 1:
                mmiop->writefn (mach, paddr, value & 0xff);
                mmiop->writefn (mach, paddr + 1, (value >> 8) & 0xff);
                return;
            }
            return;
        }
        mmiop++;
    }

#ifdef RELAXED_MMIOS
    printf (">>! Write16 of 0x%04x to unimplemented MMIO address %p! Ignoring.\n", value, paddr);
#else
    fprintf (stderr, ">>! Write16 of 0x%04x to unimplemented MMIO address %p! Aborting.\n", value, paddr);
    exit (1);
#endif
}

void mmio_write8 (machine_t *mach, u32 paddr, u8 value)
{
    struct mmio *mmiop = mmios;
    while (mmiop->addr != 0) {
        if ((paddr >= mmiop->addr) && (paddr < mmiop->addr + mmiop->totalsize)) {
            if (!mmiop->writefn)
                return;
            
            switch (mmiop->elemsize) {
            case 4:
                if (!mmiop->readfn) return;
                u32 val32 = mmiop->readfn (mach, paddr & ~3);
                val32 &= ~(0xff << ((paddr & 3) << 3));
                val32 |= value << ((paddr & 3) << 3);
                mmiop->writefn (mach, paddr & ~3, val32);
                return;
            case 2:
                if (!mmiop->readfn) return;
                u16 val16 = mmiop->readfn (mach, paddr & ~1);
                val16 &= ~(0xff << ((paddr & 1) << 3));
                val16 |= value << ((paddr & 1) << 3);
                mmiop->writefn (mach, paddr & ~1, val16);
                return;
            case 1:
                mmiop->writefn (mach, paddr, value);
                return;
            }
            return;
        }
        mmiop++;
    }

#ifdef RELAXED_MMIOS
    printf (">>! Write8 of 0x%02x to unimplemented MMIO address %p! Ignoring.\n", value, paddr);
#else
    fprintf (stderr, ">>! Write8 of 0x%02x to unimplemented MMIO address %p! Aborting.\n", value, paddr);
    exit (1);
#endif
}

#define LD_FUNC(name,size,abt) \
u ## size name (machine_t *mach, cpu_t *cpu, u32 vaddr) \
{ \
    u32 paddr = mapaddr (mach, vaddr); \
    u ## size *addrp; \
\
    if (is_mmio (mach, paddr)) { \
        return mmio_read ## size (mach, cpu, paddr); \
    } else if ((addrp = resolve_addr (mach, vaddr)) != 0) { \
        return *addrp; \
    } \
\
    interrupt (cpu, IVEC_ ## abt, CPSR_mode_abt); \
    return (u ## size)-1; \
}

#define ST_FUNC(name,size) \
void name (machine_t *mach, cpu_t *cpu, u32 vaddr, u ## size value) \
{ \
    u32 paddr = mapaddr (mach, vaddr); \
    u ## size *addrp; \
\
    if (is_mmio (mach, paddr)) { \
        mmio_write ## size (mach, paddr, value); \
        return; \
    } else if ((addrp = resolve_addr (mach, vaddr)) != 0) { \
        *addrp = value; \
        return; \
    } \
\
    interrupt (cpu, IVEC_dabt, CPSR_mode_abt); \
}

LD_FUNC (ldw,  32, dabt);
LD_FUNC (ldh,  16, dabt);
LD_FUNC (ldb,  8,  dabt);
LD_FUNC (ldwi, 32, pabt);

ST_FUNC (stw, 32);
ST_FUNC (sth, 16);
ST_FUNC (stb, 8);

int cond (cpu_t *cpu, int ccode) 
{
    int neg = (ccode & 1);
    int truth = 0;
    int Z = !!(cpu->r[CPSR] & CPSR_Z);
    int C = !!(cpu->r[CPSR] & CPSR_C);
    int N = !!(cpu->r[CPSR] & CPSR_N);
    int V = !!(cpu->r[CPSR] & CPSR_V);

    switch (ccode >> 1) {
    case 0: /* EQ/NE */
        truth = Z;
        break;
    case 1: /* CS/CC */
        truth = C;
        break;
    case 2: /* MI/PL */
        truth = N;
        break;
    case 3: /* VS/VC */
        truth = V;
        break;
    case 4: /* HI/LS */
        truth = C && !Z;
        break;
    case 5: /* GE/LT */
        truth = (N == V);
        break;
    case 6: /* GT/LE */
        truth = (Z && (N == V));
        break;
    case 7: /* AL/[NV] */
        truth = 1;
        break;
    }

    return truth ^ neg;
}

u32 barrel_shift (cpu_t *cpu, u32 shdesc, int *cflagp) 
{
    int regB   = (shdesc >> 0) & 0xf;
    int sh_imm = !!(shdesc & (1 << 4));
    int shtype = (shdesc >> 5) & 3;
    int shift  = 0;

    if (sh_imm) {
        shift  = cpu->r[reg (cpu, (shdesc >> 8) & 0xf)] & 0xff;
        if (!shift) shtype = 0;
    } else {
        shift  = (shdesc >> 7) & 0x1f;
    }
    u32 nsopB  = cpu->r[reg (cpu, regB)];
    int cftmp;
    u32 opB;
    if (!cflagp) cflagp = &cftmp;

    switch (shtype) {
    case 0: // lsl
        opB = nsopB << shift;
        if (shift != 0)
            *cflagp = ((nsopB << (shift - 1)) >> 31);
        break;
    case 1: // lsr
        if (shift == 0) shift = 32;
        opB = nsopB >> shift;
        *cflagp = ((nsopB >> (shift - 1)) & 1);
        break;
    case 2: // asr
        if (shift == 0) shift = 32;
        opB = nsopB >> shift;
        *cflagp = ((nsopB >> (shift - 1)) & 1);
        if (nsopB & 0x80000000)
            opB |= 0xffffffff & ~((0x80000000 >> (shift-1)) - 1);
        break;
    case 3:
        shift &= 0x1f;
        if (shift == 0) { // rrx
            *cflagp = nsopB & 1;
            int Cin = !!(cpu->r[CPSR] & CPSR_C);
            opB = (nsopB >> 1) | (Cin << 31);
        } else { // ror
            opB = (nsopB >> shift) | (nsopB << (32 - shift));
            *cflagp = ((nsopB >> (shift - 1)) & 1);
        }
        break;
    }

    return opB;
}

void usage (int exitcode) 
{
    fprintf (stderr,
             "Usage: armemu [-qvh] [-f flashimg] [-r hwver] [-e entry]\n\n"
             ""
             " Options:\n"
             "   -D img . . . Disassemble file `img'\n"
             "   -f flashimg  Load the file `flashimg' as a flash ROM image.\n"
             "   -d dramimg . Load the file `dramimg' to the beginning of SDRAM.\n"
             "   -i iramimg . Load the file `iramimg' to the beginning of IRAM.\n"
             "   -r hwver . . Emulate an iPod with hardware version 0x`hwver'.\n"
             "   -e entry . . Start executing at address 0x`entry'.\n"
             "   -q . . . . . `Quickie' mode; exit after returning from the top-level\n"
             "                subroutine.\n"
             "   -v . . . . . Increase the verbosity.\n"
             "   -h . . . . . This screen.\n");
    exit (exitcode);
}

void load_image (const char *filename_const, void *address, int maxsize) 
{
    int fileoff = 0, addroff = 0;
    
    char *filename = strdup (filename_const);

    if (strchr (filename, '@')) addroff = strtol (strchr (filename, '@') + 1, 0, 0);
    if (strchr (filename, '+')) fileoff = strtol (strchr (filename, '+') + 1, 0, 0);
    if (strchr (filename, '@')) *strchr (filename, '@') = 0;
    if (strchr (filename, '+')) *strchr (filename, '+') = 0;

    while (strlen (filename) > 0 && isspace (filename[strlen (filename) - 1]))
        filename[strlen (filename) - 1] = 0;

    errno = 0;
    int fd = open (filename, O_RDONLY);
    lseek (fd, fileoff, SEEK_SET);
    read (fd, (char *)address + addroff, maxsize);
    close (fd);
    if (errno) {
        fprintf (stderr, "Error loading %s: %s\n", filename, strerror (errno));
        exit (3);
    }
}

int main (int argc, char **argv) 
{
    machine_t *mach = malloc (sizeof(machine_t));
    mach->cpu = malloc (sizeof(cpu_t));
    mach->hw_ver = 0xB0005;
    mach->iramsize = 0x00020000;
    mach->flashsize = 0x00100000;
    mach->dramsize = 0x04000000;
    mach->iram = malloc (mach->iramsize);
    mach->dram = malloc (mach->dramsize);
    mach->flash = malloc (mach->flashsize);
    reset (mach->cpu);

    char ch;
    while ((ch = getopt (argc, argv, "D:f:i:d:r:e:q:vh")) != EOF) {
        switch (ch) {
	case 'D':
	    disassemble_image(optarg);
	    return 0;
        case 'f':
            load_image (optarg, mach->flash, mach->flashsize);
            break;
        case 'i':
            load_image (optarg, mach->iram, mach->iramsize);
            break;
        case 'd':
            load_image (optarg, mach->dram, mach->dramsize);
            break;
        case 'r':
            mach->hw_ver = strtol (optarg, 0, 16);
            break;
        case 'e':
            mach->cpu->r[PC] = strtol (optarg, 0, 16);
            break;
        case 'q':
            mach->cpu->r[LR] = mach->cpu->r[R14_fiq] = mach->cpu->r[R14_svc]
                = mach->cpu->r[R14_irq] = mach->cpu->r[R14_und] = 0xDEADC0DE;
            mach->quickie = 1;
            mach->cpu->r[PC] = 0x10000000;
            load_image (optarg, mach->dram, mach->dramsize);
            break;
        case 'v':
            mach->verbose++;
            break;
        case 'h':
            usage (0);
        case '?':
            usage (1);
        }
    }

    if (optind <= 1)
        usage (0);

    memcpy (mach->pp_ver, " A..05PP", 8);
    if (mach->hw_ver < 0x40000)
        memcpy (mach->pp_ver + 2, "20", 2);
    else if (mach->hw_ver < 0x70000)
        memcpy (mach->pp_ver + 2, "02", 2);
    else
        memcpy (mach->pp_ver + 2, "22", 2);
    mach->dramsize = ((mach->hw_ver >> 16) == 0xB)? 0x04000000 : 0x02000000;
    mach->iramsize = (mach->hw_ver > 0x70000)? 0x20000 : 0x18000;
    mach->drambase = (mach->hw_ver > 0x40000)? 0x10000000 : 0x28000000;
    mach->irambase = 0x40000000;
    mach->iram = realloc (mach->iram, mach->iramsize);
    mach->dram = realloc (mach->dram, mach->dramsize);

    fill_pipeline (mach->cpu);

    while (1) {
        execute (mach, mach->cpu);
    }

    return 0;
}
