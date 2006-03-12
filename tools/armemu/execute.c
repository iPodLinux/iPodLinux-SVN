#include "armemu.h"
#include <stdarg.h>

static int printing_disasms = 1;

void dprintf (const char *fmt, ...) 
{
    va_list ap;
    va_start (ap, fmt);
    if (printing_disasms)
        vprintf (fmt, ap);
    va_end (ap);
}

void execute (machine_t *mach, cpu_t *cpu) 
{
    u32 instr;
    u32 mode = cpu->r[CPSR] & CPSR_mode;
    int PCup = 0, PCmod = 0;

    instr = ldwi (mach, cpu, cpu->r[PC] - 8);

    if ((cpu->r[CPSR] & CPSR_mode) != mode) { // ldwi caused a pabt
        return; // it'll be picked up next cycle
    }

    char *disasm = disassemble (instr, cpu->r[PC] - 8);
    dprintf ("%08x:   %08x\t%s\n", cpu->r[PC] - 8, instr, disasm);
    free (disasm);

    if (!cond (cpu, instr >> 28))
        goto skip; // instr not to be executed

    // BX
    if ((instr & 0x0ffffff0) == 0x012ff10) {
        cpu->r[PC] = cpu->r[reg (cpu, instr & 0xf)];
        if (cpu->r[PC] & 1) { // Thumb
            cpu->r[CPSR] |= CPSR_T;
            cpu->r[PC]   &= ~1;
        } else {
            cpu->r[CPSR] &= ~CPSR_T;
        }
        PCup = 1;
        printf ("\t\tPC = %08x\n", cpu->r[PC]);
    }

    // SWI
    else if ((instr & 0x0f000000) == 0x0f000000) {
        switch (instr & 0x00ffffff) {
        case 0:
            printing_disasms = 0;
            break;
        case 1:
            printing_disasms = 1;
            break;
        case 2:
            exit (0);
            break;
        default:
            interrupt (cpu, IVEC_swi, CPSR_mode_svc);
            PCmod = 1;
        }
    }

    // B, BL
    else if ((instr & 0x0e000000) == 0x0a000000) {
        if (instr & (1 << 24)) // L
            cpu->r[reg (cpu, LR)] = cpu->r[PC] - 4;

        u32 off = (instr & 0x00ffffff) << 2;
        if (off & (1 << 25)) off |= 0xfc000000; // sign-extend
        s32 soff = (s32)off;
        
        cpu->r[PC] += soff;
        PCup = 1;
        dprintf ("\t\tPC = %08x\n", cpu->r[PC]);
    }

    // MSR, MRS
    else if (((instr & 0x0fbf0fff) == 0x010f0000) || // MRS
             ((instr & 0x0fb1fff0) == 0x0121f000) || // MSR_cxsf
             ((instr & 0x0db1f000) == 0x0120f000)) { // MSR_f
        if ((instr & 0x0fbf0fff) == 0x010f0000) { // MRS
            int src;
            if (instr & (1 << 22))
                src = SPSR;
            else
                src = CPSR;
            cpu->r[reg (cpu, (instr >> 12) & 0xf)] = cpu->r[reg (cpu, src)];
            dprintf ("\t\tr%d = %08x\n", (instr >> 12) & 0xf, cpu->r[reg (cpu, (instr >> 12) & 0xf)]);
        } else if ((instr & 0x0fbffff0) == 0x0129f000) { // MSR_cxsf
            int dest;
            if (instr & (1 << 22))
                dest = SPSR;
            else
                dest = CPSR;

            u32 val = cpu->r[reg (cpu, instr & 0xf)];
            
            if (!(cpu->r[CPSR] & 0xf)) { // not privileged
                val &= 0xf0000000;
                cpu->r[reg (cpu, dest)] = (cpu->r[reg (cpu, dest)] & ~0xf0000000) | val;
            } else {
                cpu->r[reg (cpu, dest)] = val;
            }
            dprintf ("\t\tCPSR = %08x\n", cpu->r[reg (cpu, dest)]);
        } else if ((instr & 0x0dbff000) == 0x0128f000) { // MSR_f
            int dest;
            if (instr & (1 << 22))
                dest = SPSR;
            else
                dest = CPSR;
            
            u32 val;
            if (instr & (1 << 25)) {
                int ror = (instr & 0xf00) >> 7; // >>8 *2
                val = instr & 0xff;
                val = (val >> ror) | (val << (32 - ror));
            } else {
                val = cpu->r[reg (cpu, instr & 0xf)];
            }
            val &= 0xf0000000;
            cpu->r[reg (cpu, dest)] = (cpu->r[reg (cpu, dest)] & ~0xf0000000) | val;
            dprintf ("\t\tCPSR = %08x\n", cpu->r[reg (cpu, dest)]);
        }
    }

    // MUL, MLA
    else if ((instr & 0x0fc000f0) == 0x00000090) {
        int regD   = (instr >> 16) & 0xf;
        int regN   = (instr >> 12) & 0xf;
        int regS   = (instr >>  8) & 0xf;
        int regM   = (instr >>  0) & 0xf;
        u32 valD   = cpu->r[reg (cpu, regD)];
        u32 valN   = cpu->r[reg (cpu, regN)];
        u32 valS   = cpu->r[reg (cpu, regS)];
        u32 valM   = cpu->r[reg (cpu, regM)];
        u32 result = 0;

        if (instr & (1 << 21)) { // MLA
            result = valM * valS + valN;
        } else { // MUL
            result = valM * valS;
        }

        if (instr & (1 << 20)) { // S
            int Nflag = (result & 0x80000000);
            int Zflag = !result;
            cpu->r[CPSR] = ((cpu->r[CPSR] & ~(CPSR_C | CPSR_N | CPSR_Z)) |
                            (Nflag << CPSR_Nbits) | (Zflag << CPSR_Zbits));
        }

        cpu->r[reg (cpu, regD)] = result;
        if (regD == PC)
            PCup = 1;
        dprintf ("\t\tr%d = %08x * %08x + %08x = %08x\n", regD, valM, valS, ((instr & (1 << 21))? valN : 0),
                cpu->r[reg (cpu, regD)]);
    }

    // UMULL, UMLAL, SMULL, SMLAL
    else if ((instr & 0x0f8000f0) == 0x00800090) {
        int regDhi = (instr >> 16) & 0xf;
        int regDlo = (instr >> 12) & 0xf;
        int regS   = (instr >>  8) & 0xf;
        int regM   = (instr >>  0) & 0xf;
        u32 valDhi = cpu->r[reg (cpu, regDhi)];
        u32 valDlo = cpu->r[reg (cpu, regDlo)];
        u32 valS   = cpu->r[reg (cpu, regS)];
        u32 valM   = cpu->r[reg (cpu, regM)];
        u64 result = 0;

        if (instr & (1 << 21)) // UMLAL/SMLAL
            result = ((u64)valDhi << 32) | valDlo;
        
        if (instr & (1 << 22)) { // signed
            s64 ress = (s64)result + (s64)valS * (s64)valM;
            result = (u64)ress;
        } else {
            result += (u64)valS * (u64)valM;
        }

        if (instr & (1 << 20)) { // S
            int Nflag = (result & 0x80000000);
            int Zflag = !result;
            cpu->r[CPSR] = ((cpu->r[CPSR] & ~(CPSR_C | CPSR_N | CPSR_Z | CPSR_V)) |
                            (Nflag << CPSR_Nbits) | (Zflag << CPSR_Zbits));
        }

        cpu->r[reg (cpu, regDhi)] = (result >> 32) & 0xffffffff;
        cpu->r[reg (cpu, regDlo)] = result & 0xffffffff;
        if (regDhi == PC || regDlo == PC)
            PCup = 1;
    }

    // LDR, STR, LDRB, STRB
    else if ((instr & 0x0c000000) == 0x04000000) {
        int regN   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        u32 offset = 0;
        int IPUBWL = (instr >> 20) & 0x3f;

        if (IPUBWL & (1 << 5)) { // I -> not an immediate
            offset = barrel_shift (cpu, instr & 0xfff, 0);
        } else {
            offset = instr & 0xfff;
        }

        u32 base   = cpu->r[reg (cpu, regN)];
        u32 taddr;
        u32 postinc;

        if (IPUBWL & (1 << 4)) { // P
            if (IPUBWL & (1 << 3)) // U
                taddr = base + offset;
            else
                taddr = base - offset;
            postinc = 0;
        } else {
            taddr = base;
            postinc = offset;
        }

        if (IPUBWL & (1 << 0)) { // ldr
            mode = cpu->r[CPSR] & CPSR_mode;

            if (IPUBWL & (1 << 2)) // ldrb
                cpu->r[reg (cpu, regD)] = ldb (mach, cpu, taddr);
            else
                cpu->r[reg (cpu, regD)] = ldw (mach, cpu, taddr & ~3);

            if (regD == PC)
                PCup = 1;
            if ((cpu->r[CPSR] & CPSR_mode) != mode) // caused a fault
                PCmod = 1;
            dprintf ("\t\tr%d = [%08x] = %08x\n", regD, taddr, cpu->r[reg (cpu, regD)]);
        } else { // str
            mode = cpu->r[CPSR] & CPSR_mode;

            if (IPUBWL & (1 << 2)) // strb
                stb (mach, cpu, taddr, cpu->r[reg (cpu, regD)] & 0xff);
            else
                stw (mach, cpu, taddr & ~3, cpu->r[reg (cpu, regD)]);

            if ((cpu->r[CPSR] & CPSR_mode) != mode) // caused a fault
                PCmod = 1;
            dprintf ("\t\t[%08x] = r%d = %08x\n", taddr, regD, cpu->r[reg (cpu, regD)]);
        }

        if ((IPUBWL & 0x12) != 0x10) { // write back
            if (IPUBWL & (1 << 4)) { // P (re-increment)
                cpu->r[reg (cpu, regN)] = taddr;
            } else {
                if (IPUBWL & (1 << 3)) { // U (p)
                    cpu->r[reg (cpu, regN)] = taddr + postinc;
                } else {
                    cpu->r[reg (cpu, regN)] = taddr - postinc;
                }
            }
            dprintf ("\t\tr%d = %08x\n", regN, cpu->r[reg (cpu, regN)]);
        }
    }

    // SWP
    else if ((instr & 0x0fb00ff0) == 0x01000090) {
        int regN   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        int regM   =  instr        & 0xf;
        int byte   =  instr        & (1 << 22);
        
        mode = cpu->r[CPSR] & CPSR_mode;

        if (byte) {
            cpu->r[reg (cpu, regD)] = ldb (mach, cpu, cpu->r[reg (cpu, regN)]);
            stb (mach, cpu, cpu->r[reg (cpu, regN)], cpu->r[reg (cpu, regM)]);
        } else {
            cpu->r[reg (cpu, regM)] = ldw (mach, cpu, cpu->r[reg (cpu, regN)] & ~3);
            stw (mach, cpu, cpu->r[reg (cpu, regN)] & ~3, cpu->r[reg (cpu, regM)]);
        }

        if (regD == PC)
            PCup = 1;
        if (mode != (cpu->r[CPSR] & CPSR_mode))
            PCmod = 1;
    }

    // LDRH, STRH, LDRSB, LDRSH
    else if ((instr & 0x0e000090) == 0x00000090) {
        int regN   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        u32 offset = 0;
        int PUIWL  = (instr >> 20) & 0x1f;
        int SH     = (instr >> 5) & 3;
        
        if (PUIWL & (1 << 3)) { // I
            offset = ((instr >> 4) & 0xf0) | (instr & 0xf);
        } else {
            offset = cpu->r[reg (cpu, instr & 0xf)];
        }

        u32 base   = cpu->r[reg (cpu, regN)];
        u32 taddr, postinc;

        if (PUIWL & (1 << 4)) { // P
            if (PUIWL & (1 << 3)) // U
                taddr = base + offset;
            else
                taddr = base - offset;
            postinc = 0;
        } else {
            taddr = base;
            postinc = offset;
        }

        mode = cpu->r[CPSR] & CPSR_mode;

        switch (SH) {
        case 3: // LDRSH
        case 1: // <LD|ST>RH
            if (PUIWL & (1 << 0)) { // ldrh
                cpu->r[reg (cpu, regD)] = ldh (mach, cpu, taddr & ~1);

                if ((SH & 2) && (cpu->r[reg (cpu, regD)] & 0x8000))
                    cpu->r[reg (cpu, regD)] |= 0xffff0000;
                
                if (regD == PC) PCup = 1;
            } else {
                sth (mach, cpu, taddr & ~1, cpu->r[reg (cpu, regD)]);
            }
            break;

        case 2: // LDRSB
            cpu->r[reg (cpu, regD)] = ldb (mach, cpu, taddr & ~3);
            
            if (cpu->r[reg (cpu, regD)] & 0x80)
                cpu->r[reg (cpu, regD)] |= 0xffffff00;

            if (regD == PC) PCup = 1;
            break;
        }

        if ((cpu->r[CPSR] & CPSR_mode) != mode) // caused a fault
            PCmod = 1;

        if ((PUIWL & 0x12) != 0x10) { // write back
            if (PUIWL & (1 << 4)) { // P (re-increment)
                cpu->r[reg (cpu, regN)] = taddr;
            } else {
                if (PUIWL & (1 << 3)) { // U (p)
                    cpu->r[reg (cpu, regN)] = taddr + postinc;
                } else {
                    cpu->r[reg (cpu, regN)] = taddr - postinc;
                }
            }
        }
    }

    // LDM, STM
    else if ((instr & 0x0e000000) == 0x08000000) {
        int regN   = (instr >> 16) & 0xf;
        int PUSWL  = (instr >> 20) & 0x1f;
        int reglist = instr & 0xffff;
        u32 base   = cpu->r[reg (cpu, regN)];
        u32 taddr;
        int nregs = 0;
        int r;

        for (r = 0; r < 16; r++) {
            if (reglist & (1 << r))
                nregs++;
        }
        
        // Here, `base' is updated to what will be written back
        // if W is set, and `taddr' is the actual base address
        // for the transfer.

        switch (PUSWL >> 3) {
        case 0: // LDMFA, LDMDA, STMED, STMDA - post-decrement
            base -= (nregs << 2);
            taddr = (base + 4) & ~3;
            break;
        case 1: // LDMFD, LDMIA, STMEA, STMIA - post-increment
            taddr = base & ~3;
            base += (nregs << 2);
            break;
        case 2: // LDMEA, LDMDB, STMFD, STMDB - pre-decrement
            base -= (nregs << 2);
            taddr = base & ~3;
            break;
        case 3: // LDMED, LDMIB, STMFA, STMIB - pre-increment
            taddr = (base + 4) & ~3;
            base += (nregs << 2);
            break;
        }

        mode = cpu->r[CPSR] & CPSR_mode;

        int transferring_PC = (reglist & (1 << 15));
        for (r = 0; r < 16; r++) {
            if (reglist & (1 << r)) {
                if (PUSWL & 1) { // load
                    if ((PUSWL & (1 << 2)) && !transferring_PC) // user bank transfer
                        cpu->r[r] = ldw (mach, cpu, taddr);
                    else {
                        cpu->r[reg (cpu, r)] = ldw (mach, cpu, taddr);
                        dprintf ("\t\tr%d = [%08x] = %08x\n", r, taddr, cpu->r[reg (cpu, r)]);
                        if ((PUSWL & (1 << 2)) && (r == PC)) {
                            cpu->r[CPSR] = cpu->r[reg (cpu, SPSR)];
                            dprintf ("\t\tCPSR = %08x\n", cpu->r[CPSR]);
                        }
                    }
                } else { // store
                    if (r == PC) // PC is stored as this instr's addr + 12
                        stw (mach, cpu, taddr, cpu->r[PC] + 4);
                    else if (PUSWL & (1 << 2)) // user bank transfer
                        stw (mach, cpu, taddr, cpu->r[r]);
                    else
                        stw (mach, cpu, taddr, cpu->r[reg (cpu, r)]);
                    dprintf ("\t\t[%08x] = r%d = %08x\n", taddr, r, cpu->r[reg (cpu, r)]);
                }

                if (r == PC)
                    PCup = 1;
                
                taddr += 4;
            }
        }

        if ((cpu->r[CPSR] & CPSR_mode) != mode)
            PCmod = 1;

        if (PUSWL & (1 << 1)) { // (W)rite-back
            cpu->r[reg (cpu, regN)] = base;
            dprintf ("\t\tr%d = %08x\n", regN, base);
        }
    }

    // ALU instrs
    else if ((instr & 0x0c000000) == 0) {
        int is_imm = !!(instr & (1 << 25));
        int setflg = !!(instr & (1 << 20));
        int opcode = (instr >> 21) & 0xf;
        int regA   = (instr >> 16) & 0xf;
        int regD   = (instr >> 12) & 0xf;
        u32 opB    = 0;
        int LCflag = -1;
        int ACflag = -1;
        int Zflag  = -1;
        int Vflag  = -1;
        int Nflag  = -1;
        u32 result = 0;
        
        if (is_imm) {
            u32 imm = (instr & 0xff);
            int ror = (instr & 0xf00) >> 7; // >>8 *2
            opB = (imm >> ror) | (imm << (32 - ror));
        } else {
            opB = barrel_shift (cpu, instr & 0xfff, &LCflag);
        }

        u32 opA = cpu->r[reg (cpu, regA)];
        int writeres = 1;
        int arith = 0;

        // do the op
        switch (opcode) {
        case 8: /* TST */
            writeres = 0;
        case 0: /* AND */
            result = opA & opB;
            break;
            
        case 9: /* TEQ */
            writeres = 0;
        case 1: /* EOR */
            result = opA ^ opB;
            break;
            
        case 0xC: /* ORR */
            result = opA | opB;
            break;

        case 0xA: /* CMP */
            writeres = 0;
            opA   -= !!(cpu->r[CPSR] & CPSR_C) - 1; // counteract SBC below
        case 6: /* SBC */
            opA   += !!(cpu->r[CPSR] & CPSR_C) - 1;
        case 2: /* SUB */
            result = opA - opB; arith = 1;
            ACflag = !(result > opA);
            Vflag  = !!((opA ^ opB) & (opA ^ result) & 0x80000000);
            break;
        case 7: /* RSC */
            opB   += !!(cpu->r[CPSR] & CPSR_C) - 1;
        case 3: /* RSB */
            result = opB - opA; arith = 1;
            ACflag = !(result > opB);
            Vflag  = !!((opB ^ opA) & (opB ^ result) & 0x80000000);
            break;

        case 0xB: /* CMN */
            writeres = 0;
            opB   -= !!(cpu->r[CPSR] & CPSR_C); /* counteract ADC below */
        case 5: /* ADC */
            opB   += !!(cpu->r[CPSR] & CPSR_C);
        case 4: /* ADD */
            result = opA + opB; arith = 1;
            ACflag = (result < opA);
            Vflag  = !!((opA ^ opB ^ 0x80000000) & (result ^ opB) & 0x80000000);
            break;

        case 0xD: /* MOV */
            result = opB;
            break;
        case 0xE: /* BIC */
            result = opA & ~opB;
            break;
        case 0xF: /* MVN */
            result = ~opB;
            break;
        }

        char ops[] = "&^-_+asr&^-+|mbM";
        if (writeres)
            dprintf ("\t\tr%d = r%d %c opB = %08x %c %08x = %08x\n",
                    regD, regA, ops[opcode], opA, ops[opcode], opB, result);
        else
            dprintf ("\t\tset flags for r%d %c opB = %08x %c %08x = %08x\n",
                    regA, ops[opcode], opA, ops[opcode], opB, result);

        // update flags
        if (setflg) {
            if (regD == PC) {
                cpu->r[CPSR] = cpu->r[reg (cpu, SPSR)];
            } else {
                int Cflag = (arith? ACflag : LCflag);
                int flgmask;
                
                Nflag = !!(result & 0x80000000);
                Zflag = !result;
                
                flgmask = (((Cflag != -1) << CPSR_Cbits) |
                           ((Vflag != -1) << CPSR_Vbits) |
                           ((Zflag != -1) << CPSR_Zbits) |
                           ((Nflag != -1) << CPSR_Nbits));
                
                cpu->r[CPSR] = ((cpu->r[CPSR] & ~flgmask) |
                                ((((Cflag & 1) << CPSR_Cbits) |
                                  ((Vflag & 1) << CPSR_Vbits) |
                                  ((Zflag & 1) << CPSR_Zbits) |
                                  ((Nflag & 1) << CPSR_Nbits)) & flgmask));
            }
        }
        
        // write result
        if (writeres) {
            cpu->r[reg (cpu, regD)] = result;
            if (regD == PC)
                PCup = 1;
        }
    }
    
    // undefined instr
    else {
        interrupt (cpu, CPSR_mode_und, IVEC_und);
        PCmod = 1;
    }

 skip:
    if (PCup)
        fill_pipeline (cpu);
    else if (!PCmod)
        cpu->r[PC] += 4;

    return;
}
