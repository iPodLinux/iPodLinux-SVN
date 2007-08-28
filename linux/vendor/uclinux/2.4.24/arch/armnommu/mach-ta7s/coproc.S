/*
 * coproc.S	Functions to read and write CP15
 *         
 * copyright:
 *         (C) 2003 Triscend Corp. (www.triscend.com)
 * authors: Craig Hackney (craig@triscend.com)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

    .align

    .globl  __writeCPR
__writeCPR:
    CMP     R0, #0
    BNE     reg1
    MCR     p15,0, R1, c0, c0
    B       done
reg1:
    CMP     R0, #1
    BNE     reg2
    MCR     p15,0, R1, c1, c0
    B       done
reg2:
    CMP     R0, #2
    BNE     reg3
    MCR     p15,0, R1, c2, c0
    B       done
reg3:
    CMP     R0, #3
    BNE     reg4
    MCR     p15,0, R1, c3, c0
    B       done
reg4:
    CMP     R0, #4
    BNE     reg5
    MCR     p15,0, R1, c4, c0
    B       done
reg5:
    CMP     R0, #5
    BNE     reg6
    MCR     p15,0, R1, c5, c0
    B       done
reg6:
    CMP     R0, #6
    BNE     reg7
    MCR     p15,0, R1, c6, c0
    B       done
reg7:
    CMP     R0, #7
    BNE     done
    MCR     p15,0, R1, c7, c0
done:
#ifdef __THUMB__
    BX      LR
#else
    MOV     PC,LR
#endif

    .globl  __readCPR
__readCPR:
    CMP     R0, #0
    BNE     readreg1
    MRC     p15,0, R0, c0, c0
    B       done
readreg1:
    CMP     R0, #1
    BNE     readreg2
    MRC     p15,0, R0, c1, c0
    B       done
readreg2:
    CMP     R0, #2
    BNE     readreg3
    MRC     p15,0, R0, c2, c0
    B       done
readreg3:
    CMP     R0, #3
    BNE     readreg4
    MRC     p15,0, R0, c3, c0
    B       done
readreg4:
    CMP     R0, #4
    BNE     readreg5
    MRC     p15,0, R0, c4, c0
    B       done
readreg5:
    CMP     R0, #5
    BNE     readreg6
    MRC     p15,0, R0, c5, c0
    B       done
readreg6:
    CMP     R0, #6
    BNE     readreg7
    MRC     p15,0, R0, c6, c0
    B       done
readreg7:
    CMP     R0, #7
    BNE     readregdone
    MRC     p15,0, R0, c7, c0
readregdone:
    B       done

    .globl  __writeCPRSub
__writeCPRSub:
    CMP     R1, #0
    BNE     writesubreg1
    MCR     p15,0, R2, c6, c0
    B       done
writesubreg1:
    CMP     R1, #1
    BNE     writesubreg2
    MCR     p15,0, R2, c6, c1
    B       done
writesubreg2:
    CMP     R1, #2
    BNE     writesubreg3
    MCR     p15,0, R2, c6, c2
    B       done
writesubreg3:
    CMP     R1, #3
    BNE     writesubreg4
    MCR     p15,0, R2, c6, c3
    B       done
writesubreg4:
    CMP     R1, #4
    BNE     writesubreg5
    MCR     p15,0, R2, c6, c4
    B       done
writesubreg5:
    CMP     R1, #5
    BNE     writesubreg6
    MCR     p15,0, R2, c6, c5
    B       done
writesubreg6:
    CMP     R1, #6
    BNE     writesubreg7
    MCR     p15,0, R2, c6, c6
    B       done
writesubreg7:
    CMP     R1, #7
    BNE     writesubdone
    MCR     p15,0, R2, c6, c7
writesubdone:
    B       done

    .globl  __readCPRSub
__readCPRSub:
    CMP     R1, #0
    BNE     readsubreg1
    MRC     p15,0, R0, c6, c0
    B       done
readsubreg1:
    CMP     R1, #1
    BNE     readsubreg2
    MRC     p15,0, R0, c6, c1
    B       done
readsubreg2:
    CMP     R1, #2
    BNE     readsubreg3
    MRC     p15,0, R0, c6, c2
    B       done
readsubreg3:
    CMP     R1, #3
    BNE     readsubreg4
    MRC     p15,0, R0, c6, c3
    B       done
readsubreg4:
    CMP     R1, #4
    BNE     readsubreg5
    MRC     p15,0, R0, c6, c4
    B       done
readsubreg5:
    CMP     R1, #5
    BNE     readsubreg6
    MRC     p15,0, R0, c6, c5
    B       done
readsubreg6:
    CMP     R1, #6
    BNE     readsubreg7
    MRC     p15,0, R0, c6, c6
    B       done
readsubreg7:
    CMP     R1, #7
    BNE     readsubdone
    MRC     p15,0, R0, c6, c7
readsubdone:
    B       done

