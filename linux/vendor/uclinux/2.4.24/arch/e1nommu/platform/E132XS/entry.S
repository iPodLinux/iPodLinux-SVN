/*
 *  arch/e1nommu/platform/E132XS/entry.S
 *
 *  Copyright (C) 2002 GDT,  George Thanos<george.thanos@gdt.gr>
 *                           Yannis Mitsos<yannis.mitsos@gdt.gr>
 *
 *  Each process has its own Kernel Register Stack and its own Kernel 
 *  Aggregate Stack. In user mode each process also has a User Register 
 *  Stack and a User Aggregate Stack. In general a process affords four(4)
 *  stacks.
 *  When running in User Mode and a interrupt, exception or system
 *  call takes place, we have to switch from User Register Stack to 
 *  Kernel Register Stack and from User Aggregate Stack to Kernel
 *  Aggregate Stack. None of these two stack switchings are supported by h/w
 *  thus we have to do them explicitely in s/w.
 */

#include <linux/sys.h>
#include <asm/errno.h>
#include <asm/stack.h>

/* Clear interrupts */
.macro  _CLI
	ori   SR, 0x8000
.endm
/* Set interrupts */
.macro  _STI
	andni SR, 0x8000
.endm
/* Set H flag */
.macro  _STH
	ori   SR, 0x20
.endm

/* These are the h/w interrupt entries 
 * Processor should reference these entries
 * in the h/w trap entry table.
 */
.global _Common_Int_Handler
.global _Common_Exception_Handler
.global _Common_SysCall_Handler
.global _ret_from_vfork

_Uninitialised:
	frame L2, L0
	call  L2, PC, _Uninitialised_Exception_Entry
	ret PC, L0

_INT1:
	frame L6, L0
	movi  L3, 0               # L3 register contains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_INT2:
	frame L6, L0
	movi  L3, 1               # L3 register contains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_INT3:
	frame L6, L0
	movi  L3, 2               # L3 register coontains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_INT4:
	frame L6, L0
	movi  L3, 3               # L3 register contains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_IO1:
	frame L6, L0
	movi  L3, 4               # L3 register contains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_IO2:
	frame L6, L0
	movi  L3, 5               # L3 register contains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_IO3:
	frame L6, L0
	movi  L3, 6               # L3 register contains the IRQ number
	movi  PC, (long)__Exception_Common_Entry

_Timer:
	frame L6, L0
	movi  L3, 7
	call  L6, 0, _BSP_tick
	movi  PC, (long)__Exception_Common_Entry

_SysCall:
	frame L6, L0
	movi  L3, 8 
	movi  PC, (long)__Exception_Common_Entry

_Exception57:
	frame L6, L0
	movi  L3, 9 
	movi  PC, (long)__Exception_Common_Entry

_Exception58:
	frame L6, L0
	movi  L3, 10
	movi  PC, (long)__Exception_Common_Entry

_Exception59:
	frame L6, L0
	movi  L3, 11
	movi  PC, (long)__Exception_Common_Entry

#define EXTEND_STACKS 1
_Exception60:
#if EXTEND_STACKS
	_STH
	mov L2, UB
	addi L2, STACK_OFFSET /* We allocate a space of 16 registers  or 64 bytes */
	_STH
	mov UB, L2
	addi G5, -STACK_OFFSET
#endif
	frame L6, L0
	movi  L3, 12
	movi  PC, (long)__Exception_Common_Entry

_Exception63:
	frame L6, L0
	movi  L3, 13
	movi  PC, (long)__Exception_Common_Entry

_ExceptionFP:
	frame L6, L0
	movi  L3, 14
	movi  PC, (long)__Exception_Common_Entry

/*****************************************************************
 * This is the starting point for every exception, interrupt,
 * system call. We have assigned a frame of 6 regsiters.
 * They are used as following : 
 * L0 : interrupted tasks PC
 * L1 : interrupted tasks SR
 * L2 : Info setadr instr provides for the current frame.
 *		Can be viewed as the current SP.
 * L3 : IRQ Number, Exception Number, System-Call.
 * L4 : The address that L0 register is mapped to, for the
 *		interrupted frame. Can be viewed as the SP of the
 * 		interrupted frame.
 * L5 : The FL of the interrupt frame.
 *****************************************************************/

__Exception_Common_Entry:
# Get the address and frame length of interrupted frame 
# L4 Address of register L0 memory map
# L5 Frame Length
	mov    L5, L1
	sari   L5, 21
	andni  L5, 0xfffffff0	# L5: interrupted tasks FL.
	cmpi   L5, 0			# Check if L4 EQ 0 (L4 = 16)
	bnz    __Estimate_FP
	movi   L5, 16
__Estimate_FP:
	setadr L4 				# Get memory position of current L0 register
	shli   L5, 2			# Registers in memory are word alligned  (FL<<2)
	sub    L4, L5			# Subtract prev FL to get L0 mem position of interrupted task
	sari   L5, 2			# Restore FL (FL>>2)
	setadr L2				# L2 contains the saved SP

# Push all local registers to memory
	call   L6, 0, _Flush_Register_Context

/* Check if we come from an interrupt/system call,
 * or an exception.
 */
	cmpi   L3, 8
	ble    __Did_int_syscall_happened_in_usermode
	br     __Did_exception_happened_in_usermode

/* Check if we were in kernel mode or in user mode,
 * when interrupt-system call  happened.
 * If we were in user mode then switch stacks, else
 * proceed by calling the common interrupt handling routine.
 */
__Did_int_syscall_happened_in_usermode:
	cmpbi L0, 0x1
	bz __SwitchTo_KernelStacks
	br __Branch_To_Handler

/* Check if we were in kernel mode or in user mode,
 * when exception happened.
 * - If we were in user mode then switch stacks
 * - If we were in kernel mode then:
 * 	 1. Check if we used the kernel aggregate stack.
 *      If not move from user to kernel aggregate stack.
 * 	 2. Check if we used the kernel register stack.
 *      If not move from user to kernel register stack.
 * - Branch to the appropriate exception handler.
 */
__Did_exception_happened_in_usermode:
	cmpbi L0, 0x1
	bz __SwitchTo_KernelStacks
__CheckIfInKernelAggrStack:
	call  L6, 0, _AreWeInKernelAggrStack
	cmpbi L5, 0x1
	bz  __SwitchTo_KernelAggregateStack
__CheckIfInKernelRegisterStack:
	call  L6, 0, _AreWeInKernelRegisterStack
	cmpbi L5, 0x1
	bz  __SwitchTo_KernelRegisterStack
	br __Branch_To_Handler

__Branch_To_Handler:
	cmpi L3, 7
	ble  __Int_Handler
	cmpi L3, 8
	ble  __SysCall_Handler
	cmpi L3, 14
	ble  __Exception_Handler
/* We should never get here! */
	_STI
/* We should never get here! */
	_STI
	_STH
	movi FCR, 0x77ffffB9
	trap 16 # Just to be sure!

__Int_Handler:
	call L6, 0, _do_IRQ
	cmpbi   L0,	0x1
#- if interrupt happened in user mode
	bz  _ret_from_intr
#- else...
	br  __Return_Final

/* Entry point for the system-calls.
 * We have already saved the register context
 * L4 points to the address of the saved stack,
 * while L5 specifies the number of the saved registers
 */
__SysCall_Handler:
#if 1	/* in case we need to debug the Syscalls we may have to make this 0 */
/* Check for the number of the system call */
	frame	L8, L0 	# We need 2 more registers
	mov 	L6, L5	# L5 contains the Frame Length
	addi	L6, -2	# system-call entry is at L[FL-2]	
	shli	L6, 2	# All entries are word aligned *2
	add		L6, L4	# Add the entry to the base L (regs->L)
	ldw.r	L6, L7	# L7 contains the system-call number saved in L[FL-2]
	cmpi	L7, NR_syscalls
	bgt		_bad_sys	#invalid syscall number
/* Set the default return value */
	addi	L6, +4	# RETURN_VALUE is located in L[FL-1]		
	movi	L7, -ENOSYS
	stw.r	L6, L7	# The value is saved in the L[FL-1]
	addi	L6, -4	
	ldw.r	L6, L7	#  L7 contains the system-call number
/* load syscall table address. */
	movi	L6, _SysCall_Table # Get the base address of the SysCall table
	shli	L7, 2	# Multiply x4 the syscall number, since the table 4 byte aligned
	add		L6, L7  # Add the offset to the SysCall table
	ldw.r	L6, L7	# Now L7 points to the address of the syscall to be executed
	cmpi	L7, 0	# Just to be sure that we are not having a NULL pointer
	bz		_bad_sys
/* Call the system call */
	_STI
	call	L6, L7, 0
/****************************************************************
 * The following code is equivelant to :            		*
 * if(current->thread.vfork_ret_info.ret_from_vfork) {		*
 * 	L[1] = current->thread.vfork_ret_info.ReturnPC;		*
 *	L[2] = current->thread.vfork_ret_info.ReturnSR;		*
 *	current->thread.vfork_ret_info.ret_from_vfork = 0;	*
 *	}							*
 ****************************************************************/
	ldw.a 	0 , L7, __current_task
	ldw.d	L7, L8, 0x2b4 
	cmpi	L8, 0 
	be      __ret_normal
	ldw.a 	0 , L7, __current_task
	ldw.d	L7, L8, 0x2b8 /* ReturnPC */ 
	stw.d	L4, L8, 4
	ldw.a 	0 , L7, __current_task
	ldw.d	L7, L8, 0x2bc /* ReturnPC */ 
	stw.d	L4, L8, 8 
	ldw.a 	0 , L7, __current_task
	movi    L8, 0
	stw.d   L7, L8, 0x2b4
__ret_normal: 
/****************************************************/
#else
	call L6, 0, _Common_SysCall_Handler
#endif
	cmpbi	L0,	0x1
#- if system call happened in user mode
	bz  _ret_from_sys_call
#- else...
	br  __Return_Final

_bad_sys:
	addi	L6, +4	# RETURN_VALUE is located in L[FL-1]
	movi	L7, -ENOSYS
	cmpbi	L0,	0x1
#- if system call happened in user mode
	bz  _ret_from_sys_call
#- else...
	br  __Return_Final

__Exception_Handler:
	call   L6, 0, _Common_Exception_Handler
	cmpbi  L0, 0x1
#- if exception happened in user mode
	bz  _ret_from_exception
#- else...
	br  _ret_from_exception_happened_in_kernel_mode


_ret_from_intr:
_ret_from_sys_call:
_ret_from_exception:
	ldw.a 	0 , L2, __current_task # If not nested, check <current->need_resched>
	ldw.d	L2, L3, 20
	cmpi	L3, 1 
	bz		_reschedule
	ldw.d	L2, L3, 8 # sigpending is the 3rd element in the task_struct 
	cmpi	L3, 0
	bnz		_signal_return
	br		__SwitchBack_ToUserStacks

_reschedule:
	call	L6, 0, _schedule
	br		_ret_from_sys_call

_signal_return:
	_STI
	call	L6, 0, _do_signal
	br		__SwitchBack_ToUserStacks

/* When child returns from vfork the value L[FL-1] should
 * be set to zero. L[FL-1] is the return value of vfork to
 * the child's register stack.
 */
_ret_from_vfork:
	cmpbi	L0,	0x1
	bnz __Return_Final  # Exit without switching the stacks
	_CLI
	ldw.d G4, L0, -20   # Interrupted Tasks FP
	ldw.d G4, L1, -24   # Interrupted Tasks FL
	shli  L1, 2			# multiply L1 by 4
	add   L0, L1 		# L0 contains the address L[FL]
	movi  L1, 0
	stw.d L0, L1, -4	# L[FL-1] = 0 - Return value of the child should be zero
	br	__SwitchBack_ToUserStacks

_ret_from_exception_happened_in_kernel_mode:
/* Check for signals immediately since the process may be
 * terminated by the exception handler. Don't touch the scheduler.
 * Check if we changed register or aggregate stack before calling
 * the exception handler. If this is the case then, switch stacks
 * as it was before.
 */
	call    L6, 0, _ShouldWeChangeBackRegisterStack
	cmpbi   L5, 0x1
	bnz __SwitchBackTo_UserRegisterStack
_CheckIfWeNeedToChangeAggregateStack:
	call    L6, 0, _ShouldWeChangeBackAggregateStack
	cmpbi   L5, 0x1
	bnz __SwitchBackTo_UserAggregateStack
	br __Return_Final

/*
 * This is the final point of return for interrupts
 * system calls and exceptions.
 */
__Return_Final:
	ret PC, L0

/**************************************************************
 * Switching Stacks code from User to Kernel Mode
 *
 * This is the code path for switching register and aggregate
 * stacks when we come from User Mode either in an Interrupt,
 * System Call or Exception.
 **************************************************************/
__SwitchTo_KernelStacks:
# Switch from User Aggregate Stack to Kernel Aggregate Stack
	stw.d G9,G3,-4
	stw.d G9,G4,-8
	stw.d G9,G5,-12
	sum   G3,G9,-12
	sum   G4,G9,-12
	sum   G5,G9,-0x1000 # KSTACK_SIZE

# Switch from User Register Stack to Kernel Register Stack
# Move Aggregate SP downwards
	addi  G3, -4
	stw.n G3, G4, -28
	sum   G4, G3,  28
	chk   G5, G3
# Push FP, FL, IRQ etc. number to Aggregate Stack
	stw.d G4, L0, -4	# Saved PC
	stw.d G4, L1, -8	# Saved SR
	stw.d G4, L2, -12	# Saved SP
	stw.d G4, L3, -16	# Saved IRQ number
	stw.d G4, L4, -20	# Saved FP
	stw.d G4, L5, -24	# Saved FL
	_STH
	mov   L0, UB		# Get UB
	stw.d G4, L0, -28	# Saved UB
# Validate Interrupt Register Stack
# Prepare saved PC value
	movi  L0, __Load_Param1		# Saved PC
	ori   L0, 0x1 	    		# Supervisor Mode
# Prepare saved SR value
	mov   L1, SR
	andni L1, 0xffF80000	# Clear FP, FL, ILC
	ori   L1, 0x00c00000	# Set FL=6
	mov   L2, G11			# SP
	andni L2, 0xfffffe03	# Get FP (bits 2..8) from SP
	shli  L2, 23			# Shift left by 23 to match SRs FP
	or    L1, L2			# Get the new FP to saved SR

# Set SP, UB
	mov   L2, G11			# G11 is the interrupt register SP
	_STH
	mov   SP, L2			# Set SP

	mov   L2, G11			# G11 is the interrupt register SP
	addi  L2, 0x1000		# Get UB value
	_STH
	mov   UB, L2			# Set UB
# Return
	ret   PC, L0			# Validate Register Stack

# Load parameters for Ints/SysCalls from Aggregate Stack 
__Load_Param1:
	ldw.d G4, L0, -4    # Be aware of prev User/Supervisor Mode
	ldw.d G4, L3, -16   # IRQ Number
	ldw.d G4, L4, -20   # Interrupted Tasks FP
	ldw.d G4, L5, -24   # Interrupted Tasks FL
# Finally branch to the appropriate handler
	br __Branch_To_Handler
#end of __SwitchTo_KernelStacks

/******************************************************************
 * __SwitchTo_KernelAggregateStack
 * Only switch from User Aggregate Stack to Kernel Aggr Stack
 * Code only Used in exceptions. When, exception happens in kernel
 * mode, but still user mode stacks are used, we have to
 * switch to kernel stacks before proceeding.
 *
 * __SwitchTo_KernelRegisterStack has same functionality but for
 * the register stack.
 ******************************************************************/
__SwitchTo_KernelAggregateStack:
	stw.d G9,G3,-4
	stw.d G9,G4,-8
	stw.d G9,G5,-12
	sum   G3,G9,-12
	sum   G4,G9,-12
	sum   G5,G9,-0x1000 # KSTACK_SIZE
# Proceed to check if RegisterStack
	br __CheckIfInKernelRegisterStack
#end of __SwitchTo_KernelAggregateStack

/******************************************************************
 * Switch from User Register Stack to Kernel Register Stack 
 ******************************************************************/
__SwitchTo_KernelRegisterStack:
# Move Aggregate SP downwards
	addi  G3, -4
	stw.n G3, G4, -28
	sum   G4, G3,  28
	chk   G5, G3
# Push FP, FL, IRQ etc. number to Aggregate Stack
	stw.d G4, L0, -4	# Saved PC
	stw.d G4, L1, -8	# Saved SR
	stw.d G4, L2, -12	# Saved SP
	stw.d G4, L3, -16	# Saved IRQ number
	stw.d G4, L4, -20	# Saved FP
	stw.d G4, L5, -24	# Saved FL
	_STH
	mov   L0, UB		# Get UB
	stw.d G4, L0, -28	# Saved UB
# Validate Interrupt Register Stack
# Prepare saved PC value
	movi  L0, __Load_Param2		# Saved PC
	ori   L0, 0x1 	    		# Supervisor Mode
# Prepare saved SR value
	mov   L1, SR
	andni L1, 0xffF80000	# Clear FP, FL, ILC
	ori   L1, 0x00c00000	# Set FL=6
	mov   L2, G11			# SP
	andni L2, 0xfffffe03	# Get FP (bits 2..8) from SP
	shli  L2, 23			# Shift left by 23 to match SRs FP
	or    L1, L2			# Get the new FP to saved SR

# Set SP, UB
	mov   L2, G11			# G11 is the interrupt register SP
	_STH
	mov   SP, L2			# Set SP

	mov   L2, G11			# G11 is the interrupt register SP
	addi  L2, 0x1000		# Get UB value
	_STH
	mov   UB, L2			# Set UB
# Return
	ret   PC, L0			# Validate Register Stack

# Load parameters for Ints/SysCalls from Aggregate Stack 
__Load_Param2:
	ldw.d G4, L0, -4    # Be aware of prev User/Supervisor Mode
	ldw.d G4, L1, -8
	ldw.d G4, L2, -12
	ldw.d G4, L3, -16   # IRQ Number
	ldw.d G4, L4, -20   # Interrupted Tasks FP
	ldw.d G4, L5, -24   # Interrupted Tasks FL
	br __Branch_To_Handler
#end of __SwitchTo_KernelRegisterStack

/******************************************************************
 * Switching stacks code from kernel to user stack.
 *
 * This is the main code for switching from user register and
 * aggregate stacks to kernel stacks. First switching register stack
 * and then aggregate stack.
 ******************************************************************/
__SwitchBack_ToUserStacks:
	_CLI
# Restore Info from Aggregate Stack
	movi  L0, __PushValuesTo_UserRegisterStack1
	ori   L0, 0x1			# S flag/Return in Supervisor mode
	mov   L1, SR			# Get current SR
	andni L1, 0xffF80000	# Delete FP, FL
	ori   L1, 0x00c00000	# Set FL=6

	ldw.d G4, L2, -12		# Get Saved SP
	andni L2, 0xffFFfe03	# Get the SR.FP field from SP
	shli  L2, 23
	or    L1, L2			# Set new FP

	ldw.d G4, L2, -12		# Get Saved SP
	_STH	
	mov   SP, L2			# Set SP

	ldw.d G4, L2, -28		# UB 
	_STH	
	mov   UB, L2			# Set UB
#Return  NOW!!!
	ret PC, L0

.balign 4
__PushValuesTo_UserRegisterStack1:
	ldw.d G4, L0, -4
	ldw.d G4, L1, -8

# Switch from Kernel Aggregate Stack to User Aggregate Stack
	ldw.d G9,G3, -4
	ldw.d G9,G4, -8
	ldw.d G9,G5, -12
	br __Return_Final
#end of __SwitchBack_ToUserStacks

/************************************************************ 
 * Switch from Kernel Register Stack to User Register Stack.
 *
 * This is only used when an exception happens in kernel mode,
 * but we still used the user stacks.
 * We have to identify this mismatch and change from user
 * to kernel stack both for the register and aggregate stack.
 *
 * When returning from exception we have to return to the exact
 * stack point that the exception took place, thus we have to
 * change stacks again from kernel to user stack.
 *
 * __SwitchBackTo_UserAggregateStack has the same functionality
 * but for the aggregate stack.
 ************************************************************/
__SwitchBackTo_UserRegisterStack:
	_CLI
# Restore Info from Aggregate Stack
	movi  L0, __PushValuesTo_UserRegisterStack2
	ori   L0, 0x1			# S flag/Return in Supervisor mode
	mov   L1, SR			# Get current SR
	andni L1, 0xffF80000	# Delete FP, FL
	ori   L1, 0x00c00000	# Set FL=6

	ldw.d G4, L2, -12		# Get Saved SP
	andni L2, 0xffFFfe03	# Get the SR.FP field from SP
	shli  L2, 23
	or    L1, L2			# Set new FP

	ldw.d G4, L2, -12		# Get Saved SP
	_STH	
	mov   SP, L2			# Set SP

	ldw.d G4, L2, -28		# UB 
	_STH	
	mov   UB, L2			# Set UB
#Return  NOW!!!
	ret PC, L0

.balign 4
__PushValuesTo_UserRegisterStack2:
	ldw.d G4, L0, -4
	ldw.d G4, L1, -8
	br _CheckIfWeNeedToChangeAggregateStack
#end of __SwitchBackTo_UserRegisterStack

/***************************************************************
 * Switch from Kernel Aggregate Stack to User Aggregate Stack.
 ***************************************************************/
__SwitchBackTo_UserAggregateStack:
	ldw.d G9,G3, -4
	ldw.d G9,G4, -8
	ldw.d G9,G5, -12
	br __Return_Final
#end of __SwitchBackTo_UserAggregateStack

/**********************************************************
 * Flush/push the local register context to memory.
 * When returning the local registers, have exactly 
 * the same context as just before calling this function.
 **********************************************************/
.global  _Flush_Register_Context
_Flush_Register_Context:
	frame  L6, L0
# Saved SP is located in L2 and equals current FP.
	setadr L2

# Move local register contents to Aggregate Stack
# Start Prologue
	addi  G3, -4
	stw.n G3, G4, -12
	sum   G4, G3,  12
	chk   G5, G3
# End Prologue
	stw.d G4, L0, -12   # Return PC
	stw.d G4, L1, -8    # Return SR
	stw.d G4, L2, -4    # SP

# Flush Register Stack to memory 
	frame L14, L0
	call  L14, PC, 0
	frame L14, L0
	call  L14, PC, 0
	frame L14, L0
	call  L14, PC, 0
	frame L14, L0

# Restore Register Context From Aggregate Stack
	ldw.d G4, L0, -12
	ldw.d G4, L1, -8
	ldw.d G4, L2, -4

	_STH
	mov   SP, L2   # Set SP
# Start Epilogue
	mov   G3, G4
	ldw.n G3, G4, 4
	ret   PC, L0	
# End Epilogue


.global _Uninitialised_Entry
.global _Exception63_Entry
.global _Exception60_Entry
.global _Exception59_Entry
.global _Exception58_Entry
.global _Exception57_Entry
.global _Exception55_Entry
.global _Exception54_Entry
.global _Exception53_Entry
.global _Exception52_Entry
.global _Exception51_Entry
.global _Exception50_Entry
.global _Exception49_Entry
.global _Exception48_Entry
.global _SysCall_Entry
.global _ExceptionFP_Entry

.balign 4
_Uninitialised_Entry: 		MOVI PC, (long)_Uninitialised 
.balign 4
_Exception63_Entry: 		MOVI PC, (long)_Exception63
/* 62 Reserved */
/* 61 Reserved */
.balign 4
_Exception60_Entry: 		MOVI PC, (long)_Exception60
.balign 4
_Exception59_Entry: 		MOVI PC, (long)_Exception59
.balign 4
_Exception58_Entry: 		MOVI PC, (long)_Exception58
.balign 4
_Exception57_Entry: 		MOVI PC, (long)_Exception57
/* 56 Reserved */
.balign 4
_Exception55_Entry:  		MOVI PC, (long)_Timer
.balign 4
_Exception54_Entry:  		MOVI PC, (long)_IO3
.balign 4
_Exception53_Entry:			MOVI PC, (long)_INT1
.balign 4
_Exception52_Entry: 		MOVI PC, (long)_INT2
.balign 4
_Exception51_Entry: 		MOVI PC, (long)_INT3
.balign 4
_Exception50_Entry: 		MOVI PC, (long)_INT4
.balign 4
_Exception49_Entry:  		MOVI PC, (long)_IO1
.balign 4
_Exception48_Entry:  		MOVI PC, (long)_IO2
.balign 4
_SysCall_Entry:     		MOVI PC, (long)_SysCall
.balign 4
_ExceptionFP_Entry:		MOVI PC, (long)_ExceptionFP
