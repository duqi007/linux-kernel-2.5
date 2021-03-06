/* $Id: entry.S,v 1.71 2001/07/27 11:47:50 gniibe Exp $
 *
 *  linux/arch/sh/entry.S
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/sys.h>
#include <linux/linkage.h>
#include <linux/config.h>


/*
 * Define this to turn on compatibility with the previous
 * system call ABI.  This feature is not properly maintained.
 */
#undef COMPAT_OLD_SYSCALL_ABI

! NOTE:
! GNU as (as of 2.9.1) changes bf/s into bt/s and bra, when the address
! to be jumped is too far, but it causes illegal slot exception.

/*	
 * entry.S contains the system-call and fault low-level handling routines.
 * This also contains the timer-interrupt handler, as well as all interrupts
 * and faults that can result in a task-switch.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call.
 *
 * NOTE: This code uses a convention that instructions in the delay slot
 * of a transfer-control instruction are indented by an extra space, thus:
 *
 *    jmp	@k0	    ! control-transfer instruction
 *     ldc	k1, ssr     ! delay slot
 *
 * Stack layout in 'ret_from_syscall':
 * 	ptrace needs to have all regs on the stack.
 *	if the order here is changed, it needs to be
 *	updated in ptrace.c and ptrace.h
 *
 *	r0
 *      ...
 *	r15 = stack pointer
 *	spc
 *	pr
 *	ssr
 *	gbr
 *	mach
 *	macl
 *	syscall #
 *
 */

/*
 * These are offsets into the task-struct.
 */
flags		=  4
#error sigpending	=  8
#error need_resched	= 20
#error tsk_ptrace	= 24

#error PT_TRACESYS  = 0x00000002

ENOSYS = 38
EINVAL = 22

#if defined(__sh3__)
TRA     = 0xffffffd0
EXPEVT  = 0xffffffd4
#if defined(CONFIG_CPU_SUBTYPE_SH7707) || defined(CONFIG_CPU_SUBTYPE_SH7709)
INTEVT  = 0xa4000000		! INTEVTE2(0xa4000000)
#else
INTEVT  = 0xffffffd8
#endif
MMU_TEA = 0xfffffffc		! TLB Exception Address Register
#elif defined(__SH4__)
TRA     = 0xff000020
EXPEVT  = 0xff000024
INTEVT  = 0xff000028
MMU_TEA = 0xff00000c		! TLB Exception Address Register
#endif

/* Offsets to the stack */
OFF_R0         =  0		/* Return value. New ABI also arg4 */
OFF_R1         =  4     	/* New ABI: arg5 */
OFF_R2         =  8     	/* New ABI: arg6 */
OFF_R3         =  12     	/* New ABI: syscall_nr */
OFF_R4         =  16     	/* New ABI: arg0 */
OFF_R5         =  20     	/* New ABI: arg1 */
OFF_R6         =  24     	/* New ABI: arg2 */
OFF_R7         =  28     	/* New ABI: arg3 */
OFF_SP	   =  (15*4)
OFF_SR	   =  (16*4+8)
SYSCALL_NR =  (16*4+6*4)


#define k0	r0
#define k1	r1
#define k2	r2
#define k3	r3
#define k4	r4

#define current		r7	/* r7_bank1 */
#define g_imask		r6	/* r6_bank1 */
#define k_current	r7_bank	/* r7_bank1 */
#define k_g_imask	r6_bank	/* r6_bank1 */
#define k_ex_code	r2_bank	/* r2_bank1 */

/*
 * Kernel mode register usage:
 *	k0	scratch
 *	k1	scratch
 *	k2	scratch (Exception code)
 *	k3	scratch (Return address)
 *	k4	scratch
 *	k5	reserved
 *	k6	Global Interrupt Mask (0--15 << 4)
 *	k7	CURRENT (pointer to current task)
 */

!
! TLB Miss / Initial Page write exception handling
!			_and_
! TLB hits, but the access violate the protection.
! It can be valid access, such as stack grow and/or C-O-W.
!
!
! Find the pmd/pte entry and loadtlb
! If it's not found, cause address error (SEGV)
!
! Although this could be written in assembly language (and it'd be faster),
! this first version depends *much* on C implementation.
!

#define STI()				\
	mov.l	__INV_IMASK, r11;	\
	stc	sr, r10;		\
	and	r11, r10;		\
	stc	k_g_imask, r11;	\
	or	r11, r10;		\
	ldc	r10, sr

	.align	2
tlb_miss_load:
	bra	call_dpf
	 mov	#0, r5

	.align	2
tlb_miss_store:
	bra	call_dpf
	 mov	#1, r5

	.align	2
initial_page_write:
	bra	call_dpf
	 mov	#1, r5

	.align	2
tlb_protection_violation_load:
	bra	call_dpf
	 mov	#0, r5

	.align	2
tlb_protection_violation_store:
	bra	call_dpf
	 mov	#1, r5

call_dpf:
	mov.l	1f, r0
	mov	r5, r8
	mov.l	@r0, r6
	mov	r6, r9
	mov.l	2f, r0
	sts	pr, r10
	jsr	@r0
	 mov	r15, r4
	!
	tst	r0, r0
	bf/s	0f
	 lds	r10, pr
	rts
	 nop
0:	STI()
	mov.l	3f, r0
	mov	r9, r6
	mov	r8, r5
	jmp	@r0
	 mov	r15, r4

	.align 2
1:	.long	MMU_TEA
2:	.long	__do_page_fault
3:	.long	do_page_fault

	.align	2
address_error_load:
	bra	call_dae
	 mov	#0,r5		! writeaccess = 0

	.align	2
address_error_store:
	bra	call_dae
	 mov	#1,r5		! writeaccess = 1

call_dae:
	mov.l	1f, r0
	mov.l	@r0, r6		! address
	mov.l	2f, r0
	jmp	@r0
	 mov	r15, r4		! regs

	.align 2
1:	.long	MMU_TEA
2:	.long   do_address_error

#if defined(CONFIG_SH_STANDARD_BIOS)
	.align	2
	/* Unwind the stack and jmp to the debug entry */
debug_kernel:
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	stc	sr, r8
	mov.l	1f, r9			! BL =1, RB=1, IMASK=0x0F
	or	r9, r8
	ldc	r8, sr			! here, change the register bank
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k0
	ldc.l	@r15+, spc
	lds.l	@r15+, pr
	mov.l	@r15+, k1
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	mov	k0, r15
	!
	mov.l	2f, k0
	mov.l	@k0, k0
	jmp	@k0
	 ldc	k1, ssr
	.align	2
1:	.long	0x300000f0
2:	.long	gdb_vbr_vector
#endif

	.align	2
debug_trap:	
#if defined(CONFIG_SH_STANDARD_BIOS)
	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0		! get status register
	shll	r0
	shll	r0			! kernel space?
	bt/s	debug_kernel
#endif
	 mov.l	@r15, r0
	mov.l	1f, r8
	jmp	@r8
	 nop

	.align	2
1:	.long	break_point_trap_software

	.align	2
error:	
	!
	STI()
	mov.l	1f, r0
	jmp	@r0
	 nop
	.align	2
1:	.long	do_exception_error


!
!
!
ENTRY(ret_from_fork)
	mov.l	@r15+,r0
	! Call schedule_tail
	mov.l	1f, r1
	jsr @r1
	 mov	r0, r4
	! If we're being traced, return via syscall_ret_trace, otherwise
	! return directly to ret_from_syscall
	stc	k_current, r0
#error	mov.l	@(tsk_ptrace,r0), r0	! Is current PTRACE_SYSCALL'd?
#error	mov	#PT_TRACESYS, r1
	tst	r1, r0
	bt	ret_from_syscall
	bra	syscall_ret_trace
	 nop	 

	.align	2
1:	.long	schedule_tail

/*
 * Old syscall interface:
 *
 *	Syscall #: R0
 *	Arguments #0 to #3: R4--R7
 *	more arguments: On the stack
 *	TRA: (number of arguments on the stack) x 4
 *
 * New syscall interface:
 *
 *	Syscall #: R3
 *	Arguments #0 to #3: R4--R7
 *	Arguments #4 to #6: R0, R1, R2
 *	TRA: (number of arguments + 0x10) x 4
 *
 * This code also handles delegating other traps to the BIOS/gdb stub
 * according to:
 *
 * Trap number
 * (TRA>>2) 	    Purpose
 * -------- 	    -------
 * 0x0-0xf  	    old syscall ABI
 * 0x10-0x1f  	    new syscall ABI
 * 0x20-0xff  	    delegated through debug_trap to BIOS/gdb stub.
 *
 * Note: When we're first called, the TRA value must be shifted
 * right 2 bits in order to get the value that was used as the "trapa"
 * argument.
 */

system_call:
	mov.l	__TRA, r9
	mov.l	@r9, r8
	!
	! Is the trap argument >= 0x20? (TRA will be >= 0x80)
	mov	#0x20, r9
	extu.b	r9, r9
	shll2	r9
	cmp/hs	r9, r8
	bt	debug_trap
	!
	mov	#SYSCALL_NR, r14
	add	r15, r14
	!
#ifdef COMPAT_OLD_SYSCALL_ABI
	mov	#0x40, r9
	cmp/hs	r9, r8
	bf/s	old_abi_system_call
	 nop
#endif
	! New Syscall ABI
	add	#-0x40, r8
	shlr2	r8
	shll8	r8
	shll8	r8 		    	! r8 = num_args<<16
	mov	r3, r10
	or	r8, r10	! Encode syscall # and # of arguments
	mov.l	r10, @r14		! set syscall_nr
	STI()
	!
	stc	k_current, r11
#error	mov.l	@(tsk_ptrace,r11), r10	! Is current PTRACE_SYSCALL'd?
#error	mov	#PT_TRACESYS, r11
	tst	r11, r10
	bt	5f
	!                     	Yes it is traced.
	mov.l	__syscall_trace, r11	! Call syscall_trace() which notifies
	jsr	@r11	    	    	! superior (will chomp R[0-7])
	 nop
	!			Reload R0-R4 from kernel stack, where the
	!   	    	    	parent may have modified them using
	!   	    	    	ptrace(POKEUSR).  (Note that R0-R2 are
	!   	    	    	used by the system call handler directly
	!   	    	    	from the kernel stack anyway, so don't need
	!   	    	    	to be reloaded here.)  This allows the parent
	!   	    	    	to rewrite system calls and args on the fly.
	mov.l	@(OFF_R4,r15), r4   ! arg0
	mov.l	@(OFF_R5,r15), r5
	mov.l	@(OFF_R6,r15), r6
	mov.l	@(OFF_R7,r15), r7   ! arg3
	mov.l	@(OFF_R3,r15), r3   ! syscall_nr
	!   	    	    Arrange for syscall_trace() to be called
	!   	    	    again as the system call returns.
	mov.l	__syscall_ret_trace, r10
	bra	6f
	 lds	r10, pr
	!   	    	    No it isn't traced.
	!   	    	    Arrange for normal system call return.
5:	mov.l	__syscall_ret, r10
	lds	r10, pr
	!   	    	    Call the system call handler through the table.
	!   	    	    (both normal and ptrace'd)
	!   	    	    First check for bad syscall number
6:	mov	r3, r9
	mov.l	__n_sys, r10
	cmp/hs	r10, r9
	bf	2f
	!   	    	    Bad syscall number
	rts			! go to syscall_ret or syscall_ret_trace
    	 mov	#-ENOSYS, r0
	!   	    	    Good syscall number
2:	shll2	r9		! x4
	mov.l	__sct, r11
	add	r11, r9
	mov.l	@r9, r11
	jmp	@r11	    	! jump to specific syscall handler
	 nop

	! In case of trace
syscall_ret_trace:
	mov.l	r0, @(OFF_R0,r15)		! save the return value
	mov.l	__syscall_trace, r1
	mova	ret_from_syscall, r0
	jmp	@r1    	! Call syscall_trace() which notifies superior
	 lds	r0, pr    	! Then return to ret_from_syscall()



#ifdef COMPAT_OLD_SYSCALL_ABI
! Handle old ABI system call.
! Note that ptrace(SYSCALL) is not supported for the old ABI.
! At this point:
!   r0, r4-7 as per ABI
!   r8  = value of TRA register (= num_args<<2)
!   r14 = points to SYSCALL_NR in stack frame
old_abi_system_call:
	mov 	r0, r9    	    	! Save system call number in r9
	!   	    	    	    	! arrange for return which pops stack
	mov.l	__old_abi_syscall_ret, r10
	lds	r10, pr
	!				  Build the stack frame if TRA > 0
	mov	r8, r10
	cmp/pl	r10
	bf	0f
	mov.l	@(OFF_SP,r15), r0	! get original user stack
7:	add	#-4, r10
4:	mov.l	@(r0,r10), r1	! May cause address error exception..
	mov.l	r1, @-r15
	cmp/pl	r10
	bt	7b
0:
	mov.l	r9, @r14	    	! set syscall_nr
	STI()
	!   	    	    Call the system call handler through the table.
	!   	    	    First check for bad syscall number
	mov.l	__n_sys, r10
	cmp/hs	r10, r9
	bf	2f
	!   	    	    Bad syscall number
	rts			! return to old_abi_syscall_ret
    	 mov	#-ENOSYS, r0
	!   	    	    Good syscall number
2:	shll2	r9		! x4
	mov.l	__sct, r11
	add	r11, r9
	mov.l	@r9, r11
	jmp	@r11	    	! call specific syscall handler,
	 nop

    	.align 2
__old_abi_syscall_ret:
	.long	old_abi_syscall_ret

    	! This code gets called on address error exception when copying
	! syscall arguments from user stack to kernel stack.  It is
	! supposed to return -EINVAL through old_abi_syscall_ret, but it
	! appears to have been broken for a long time in that the r0
	! return value will be saved into the kernel stack relative to r15
	! but the value of r15 is not correct partway through the loop.
	! So the user prog is returned its old r0 value, not -EINVAL.
	! Greg Banks 28 Aug 2000.
	.section	.fixup,"ax"
fixup_syscall_argerr:
    	! First get r15 back to 
	rts
    	 mov	#-EINVAL, r0
    	.previous

	.section	__ex_table, "a"
	.align	2
	.long	4b,fixup_syscall_argerr
    	.previous
#endif

	.align	2
__TRA:	.long	TRA
__syscall_trace:
#error    	.long	syscall_trace
__n_sys:.long	NR_syscalls
__sct:	.long	sys_call_table
__syscall_ret_trace:
	.long	syscall_ret_trace
__syscall_ret:
	.long	syscall_ret
__INV_IMASK:
	.long	0xffffff0f	! ~(IMASK)


	.align	2
reschedule:
	mova	ret_from_syscall, r0
	mov.l	1f, r1
	jmp	@r1
	 lds	r0, pr
	.align	2
1:	.long	schedule

ret_from_irq:
ret_from_exception:
	mov	#OFF_SR, r0
	mov.l	@(r0,r15), r0	! get status register
	shll	r0
	shll	r0		! kernel space?
	bt	restore_all	! Yes, it's from kernel, go back soon
	!
	bra	ret_from_syscall
	 nop

	.align 2
#ifdef COMPAT_OLD_SYSCALL_ABI
old_abi_syscall_ret:
	add	r8, r15	! pop off the arguments
	/* fall through */
#endif
syscall_ret:
	mov.l	r0, @(OFF_R0,r15)	! save the return value
	/* fall through */

ENTRY(ret_from_syscall)
	/* CLI */
	stc	sr, r0
	or	#0xf0, r0
	ldc	r0, sr
	!
	stc	k_current, r1
#error	mov.l	@(need_resched,r1), r0
	tst	r0, r0
	bf	reschedule
#error	mov.l	@(sigpending,r1), r0
	tst	r0, r0
	bt	restore_all
signal_return:
	mov	r15, r4
	mov	#0, r5
	mov.l	__do_signal, r1
	mova	restore_all, r0
	jmp	@r1
	 lds	r0, pr
	.align	2
__do_signal:
#error	.long	do_signal
__irq_stat:
	.long	irq_stat

	.align 2
restore_all:
	mov.l	@r15+, r0
	mov.l	@r15+, r1
	mov.l	@r15+, r2
	mov.l	@r15+, r3
	mov.l	@r15+, r4
	mov.l	@r15+, r5
	mov.l	@r15+, r6
	mov.l	@r15+, r7
	!
	stc	sr, r8
	mov.l	__blrb_flags, r9	! BL =1, RB=1
	or	r9, r8
	ldc	r8, sr			! here, change the register bank
	!
	mov.l	@r15+, r8
	mov.l	@r15+, r9
	mov.l	@r15+, r10
	mov.l	@r15+, r11
	mov.l	@r15+, r12
	mov.l	@r15+, r13
	mov.l	@r15+, r14
	mov.l	@r15+, k4		! original stack pointer
	ldc.l	@r15+, spc
	lds.l	@r15+, pr
	mov.l	@r15+, k3		! original SR
	ldc.l	@r15+, gbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	add	#4, r15			! Skip syscall number
	!
	! Calculate new SR value
	mov	k3, k2			! original SR value
	mov.l	1f, k1
	stc	sr, k0
	and	k1, k0			! Get current FD-bit
	mov.l	2f, k1
	and	k1, k2			! Mask orignal SR value
	or	k0, k2			! Inherit current FD-bit
	!
	mov	k3, k0			! Calculate IMASK-bits
	shlr2	k0
	and	#0x3c, k0
	cmp/eq	#0x3c, k0
	bt/s	7f
	 shll2	k0
	mov	g_imask, k0
	!
7:	or	k0, k2			! Set the IMASK-bits
	ldc	k2, ssr
	!
	mov	k4, r15
	rte
	 nop

	.align	2
__blrb_flags:	.long	0x30000000
1:	.long	0x00008000	! FD
2:	.long	0xffff7f0f	! ~(IMASK+FD)
3:	.long	0x00080000	! SZ=0, PR=1

! Exception Vector Base
!
!	Should be aligned page boundary.
!
	.balign 	4096,0,4096
ENTRY(vbr_base)
	.long	0
!
	.balign 	256,0,256
general_exception:
	mov.l	1f, k2
	mov.l	2f, k3
	bra	handle_exception
	 mov.l	@k2, k2
	.align	2
2:	.long	ret_from_exception
1:	.long	EXPEVT
!
!
	.balign 	1024,0,1024
tlb_miss:
	mov.l	1f, k2
	mov.l	4f, k3
	bra	handle_exception
	 mov.l	@k2, k2
!
	.balign 	512,0,512
interrupt:
	mov.l	2f, k2
	mov.l	3f, k3
	bra	handle_exception
	 mov.l	@k2, k2

	.align	2
1:	.long	EXPEVT
2:	.long	INTEVT
3:	.long	ret_from_irq
4:	.long	ret_from_exception

!
!
handle_exception:
	! Using k0, k1 for scratch registers (r0_bank1, r1_bank),
	! save all registers onto stack.
	!
	stc	ssr, k0		! Is it from kernel space?
	shll	k0		! Check MD bit (bit30) by shifting it into...
	shll	k0		!       ...the T bit
	bt/s	9f		! It's a kernel to kernel transition.
	 mov	r15, k0		! save original stack to k0
	/* User space to kernel */
	mov	#0x20, k1
	shll8	k1		! k1 <= 8192 == THREAD_SIZE
	add	current, k1
	mov	k1, r15		! change to kernel stack
	!
9:  	mov	#-1, k4
	mov.l	3f, k1
	! Save the user registers on the stack.
	mov.l	k4, @-r15	! syscall_nr (default: -1)
	!
	sts.l	macl, @-r15
	sts.l	mach, @-r15
	stc.l	gbr, @-r15
	stc.l	ssr, @-r15
	sts.l	pr, @-r15
	stc.l	spc, @-r15
	!
	lds	k3, pr		! Set the return address to pr
	!
	mov.l	k0, @-r15	! save orignal stack
	mov.l	r14, @-r15
	mov.l	r13, @-r15
	mov.l	r12, @-r15
	mov.l	r11, @-r15
	mov.l	r10, @-r15
	mov.l	r9, @-r15
	mov.l	r8, @-r15
	!
	stc	sr, r8		! Back to normal register bank, and
	or	k1, r8		! Block all interrupts
	mov.l	5f, k1
	and	k1, r8		! ...
	ldc	r8, sr		! ...changed here.
	!
	mov.l	r7, @-r15
	mov.l	r6, @-r15
	mov.l	r5, @-r15
	mov.l	r4, @-r15
	mov.l	r3, @-r15
	mov.l	r2, @-r15
	mov.l	r1, @-r15
	mov.l	r0, @-r15
	! Then, dispatch to the handler, according to the exception code.
	stc	k_ex_code, r8
	shlr2	r8
	shlr	r8
	mov.l	1f, r9
	add	r8, r9
	mov.l	@r9, r9
	jmp	@r9
	 nop
	.align	2
1:	.long	exception_handling_table
3:	.long	0x000000f0	! FD=0, IMASK=15
5:	.long	0xcfffffff	! RB=0, BL=0
6:	.long	0x00080000	! SZ=0, PR=1

none:
	rts
	 nop

.data
ENTRY(exception_handling_table)
	.long	error
	.long	error
	.long	tlb_miss_load
	.long	tlb_miss_store
	.long	initial_page_write
	.long	tlb_protection_violation_load
	.long	tlb_protection_violation_store
	.long	address_error_load
	.long	address_error_store
#if defined(__SH4__)
	.long	do_fpu_error
#else
	.long	error	! fpu_exception
#endif
	.long	error
	.long	system_call			! Unconditional Trap
	.long	error	! reserved_instruction      (filled by trap_init)
	.long	error	! illegal_slot_instruction  (filled by trap_init)
ENTRY(nmi_slot)
	.long	none				! Not implemented yet
ENTRY(user_break_point_trap)
	.long	break_point_trap
ENTRY(interrupt_table)
	! external hardware
	.long	do_IRQ			! 0000
	.long	do_IRQ			! 0001
	.long	do_IRQ			! 0010
	.long	do_IRQ			! 0011
	.long	do_IRQ			! 0100
	.long	do_IRQ			! 0101
	.long	do_IRQ			! 0110
	.long	do_IRQ			! 0111
	.long	do_IRQ			! 1000
	.long	do_IRQ			! 1001
	.long	do_IRQ			! 1010
	.long	do_IRQ			! 1011
	.long	do_IRQ			! 1100
	.long	do_IRQ			! 1101
	.long	do_IRQ			! 1110
	.long	error
	! Internal hardware
	.long	do_IRQ			! TMU0 tuni0
	.long	do_IRQ			! TMU1 tuni1
	.long	do_IRQ			! TMU2 tuni2
	.long	do_IRQ			!      ticpi2
	.long	do_IRQ			! RTC  ati
	.long	do_IRQ			!      pri
	.long	do_IRQ			!      cui
	.long	do_IRQ			! SCI  eri
	.long	do_IRQ			!      rxi
	.long	do_IRQ			!      txi
	.long	do_IRQ			!      tei
	.long	do_IRQ			! WDT  iti
	.long	do_IRQ			! REF  rcmi
	.long	do_IRQ			!      rovi
	.long	do_IRQ		
	.long	do_IRQ		
#if  defined(CONFIG_CPU_SUBTYPE_SH7707) || defined(CONFIG_CPU_SUBTYPE_SH7709)
	.long	do_IRQ			! 32 IRQ  irq0
	.long	do_IRQ			! 33      irq1
	.long	do_IRQ			! 34      irq2
	.long	do_IRQ			! 35      irq3
	.long	do_IRQ			! 36      irq4
	.long	do_IRQ			! 37      irq5
	.long	do_IRQ			! 38
	.long	do_IRQ			! 39
	.long	do_IRQ			! 40 PINT pint0-7
	.long	do_IRQ			! 41      pint8-15
	.long	do_IRQ			! 42
	.long	do_IRQ			! 43
	.long	do_IRQ			! 44
	.long	do_IRQ			! 45
	.long	do_IRQ			! 46
	.long	do_IRQ			! 47
	.long	do_IRQ			! 48 DMAC dei0
	.long	do_IRQ			! 49      dei1
	.long	do_IRQ			! 50      dei2
	.long	do_IRQ			! 51      dei3
	.long	do_IRQ			! 52 IrDA eri1
	.long	do_IRQ			! 53      rxi1
	.long	do_IRQ			! 54      bri1
	.long	do_IRQ			! 55      txi1
	.long	do_IRQ			! 56 SCIF eri2
	.long	do_IRQ			! 57      rxi2
	.long	do_IRQ			! 58      bri2
	.long	do_IRQ			! 59      txi2
	.long	do_IRQ			! 60 ADC  adi
#if defined(CONFIG_CPU_SUBTYPE_SH7707)
	.long   do_IRQ			! 61 LCDC lcdi
	.long   do_IRQ			! 62 PCC  pcc0i
	.long   do_IRQ			! 63      pcc1i
#endif
#elif defined(__SH4__)
	.long	do_IRQ			! 32 Hitachi UDI
	.long	do_IRQ			! 33 GPIO
	.long	do_IRQ			! 34 DMAC dmte0
	.long	do_IRQ			! 35      dmte1
	.long	do_IRQ			! 36      dmte2
	.long	do_IRQ			! 37      dmte3
	.long	do_IRQ			! 38      dmae
	.long	error			! 39
	.long	do_IRQ			! 40 SCIF eri
	.long	do_IRQ			! 41      rxi
	.long	do_IRQ			! 42      bri
	.long	do_IRQ			! 43      txi
	.long	error			! 44
	.long	error			! 45
	.long	error			! 46
	.long	error			! 47
	.long	do_fpu_state_restore	! 48
	.long	do_fpu_state_restore	! 49
#endif
#if defined(CONFIG_CPU_SUBTYPE_SH7751)
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	error
	.long	do_IRQ			! PCI serr
	.long	do_IRQ			!     dma3
	.long	do_IRQ			!     dma2
	.long	do_IRQ			!     dma1
	.long	do_IRQ			!     dma0
	.long	do_IRQ			!     pwon
	.long	do_IRQ			!     pwdwn
	.long	do_IRQ			!     err
#elif defined(CONFIG_CPU_SUBTYPE_ST40STB1)
	.long	error			!  50 0x840
	.long	error			!  51 0x860
	.long	error			!  52 0x880
	.long	error			!  53 0x8a0
	.long	error			!  54 0x8c0
	.long	error			!  55 0x8e0
	.long	error			!  56 0x900
	.long	error			!  57 0x920
	.long	error			!  58 0x940
	.long	error			!  59 0x960
	.long	error			!  60 0x980
	.long	error			!  61 0x9a0
	.long	error			!  62 0x9c0
	.long	error			!  63 0x9e0
	.long	do_IRQ			!  64 0xa00 PCI serr
	.long	do_IRQ			!  65 0xa20     err
	.long	do_IRQ			!  66 0xa40     ad
	.long	do_IRQ			!  67 0xa60     pwr_dwn
	.long	error			!  68 0xa80
	.long	error			!  69 0xaa0
	.long	error			!  70 0xac0
	.long	error			!  71 0xae0
	.long	do_IRQ			!  72 0xb00 DMA INT0
	.long	do_IRQ			!  73 0xb20     INT1
	.long	do_IRQ			!  74 0xb40     INT2
	.long	do_IRQ			!  75 0xb60     INT3
	.long	do_IRQ			!  76 0xb80     INT4
	.long	error			!  77 0xba0
	.long	do_IRQ			!  78 0xbc0 DMA ERR
	.long	error			!  79 0xbe0
	.long	do_IRQ			!  80 0xc00 PIO0
	.long	do_IRQ			!  81 0xc20 PIO1
	.long	do_IRQ			!  82 0xc40 PIO2
	.long	error			!  83 0xc60
	.long	error			!  84 0xc80
	.long	error			!  85 0xca0
	.long	error			!  86 0xcc0
	.long	error			!  87 0xce0
	.long	error			!  88 0xd00
	.long	error			!  89 0xd20
	.long	error			!  90 0xd40
	.long	error			!  91 0xd60
	.long	error			!  92 0xd80
	.long	error			!  93 0xda0
	.long	error			!  94 0xdc0
	.long	error			!  95 0xde0
	.long	error			!  96 0xe00
	.long	error			!  97 0xe20
	.long	error			!  98 0xe40
	.long	error			!  99 0xe60
	.long	error			! 100 0xe80
	.long	error			! 101 0xea0
	.long	error			! 102 0xec0
	.long	error			! 103 0xee0
	.long	error			! 104 0xf00
	.long	error			! 105 0xf20
	.long	error			! 106 0xf40
	.long	error			! 107 0xf60
	.long	error			! 108 0xf80
	.long	error			! 109 0xfa0
	.long	error			! 110 0xfc0
	.long	error			! 111 0xfe0
	.long	do_IRQ			! 112 0x1000 Mailbox
	.long	error			! 113 0x1020
	.long	error			! 114 0x1040
	.long	error			! 115 0x1060
	.long	error			! 116 0x1080
	.long	error			! 117 0x10a0
	.long	error			! 118 0x10c0
	.long	error			! 119 0x10e0
	.long	error			! 120 0x1100
	.long	error			! 121 0x1120
	.long	error			! 122 0x1140
	.long	error			! 123 0x1160
	.long	error			! 124 0x1180
	.long	error			! 125 0x11a0
	.long	error			! 126 0x11c0
	.long	error			! 127 0x11e0
	.long	error			! 128 0x1200
	.long	error			! 129 0x1220
	.long	error			! 130 0x1240
	.long	error			! 131 0x1260
	.long	error			! 132 0x1280
	.long	error			! 133 0x12a0
	.long	error			! 134 0x12c0
	.long	error			! 135 0x12e0
	.long	error			! 136 0x1300
	.long	error			! 137 0x1320
	.long	error			! 138 0x1340
	.long	error			! 139 0x1360
	.long	do_IRQ			! 140 0x1380 EMPI INV_ADDR
	.long	error			! 141 0x13a0
	.long	error			! 142 0x13c0
	.long	error			! 143 0x13e0
#endif

ENTRY(sys_call_table)
	.long sys_ni_syscall	/* 0  -  old "setup()" system call*/
	.long sys_exit
	.long sys_fork
	.long sys_read
	.long sys_write
	.long sys_open		/* 5 */
	.long sys_close
	.long sys_waitpid
	.long sys_creat
	.long sys_link
	.long sys_unlink	/* 10 */
	.long sys_execve
	.long sys_chdir
	.long sys_time
	.long sys_mknod
	.long sys_chmod		/* 15 */
	.long sys_lchown16
	.long sys_ni_syscall	/* old break syscall holder */
	.long sys_stat
	.long sys_lseek
	.long sys_getpid	/* 20 */
	.long sys_mount
	.long sys_oldumount
	.long sys_setuid16
	.long sys_getuid16
	.long sys_stime		/* 25 */
	.long sys_ptrace
	.long sys_alarm
	.long sys_fstat
	.long sys_pause
	.long sys_utime		/* 30 */
	.long sys_ni_syscall	/* old stty syscall holder */
	.long sys_ni_syscall	/* old gtty syscall holder */
	.long sys_access
	.long sys_nice
	.long sys_ni_syscall	/* 35 */		/* old ftime syscall holder */
	.long sys_sync
	.long sys_kill
	.long sys_rename
	.long sys_mkdir
	.long sys_rmdir		/* 40 */
	.long sys_dup
	.long sys_pipe
	.long sys_times
	.long sys_ni_syscall	/* old prof syscall holder */
	.long sys_brk		/* 45 */
	.long sys_setgid16
	.long sys_getgid16
	.long sys_signal
	.long sys_geteuid16
	.long sys_getegid16	/* 50 */
	.long sys_acct
	.long sys_umount	/* recycled never used phys() */
	.long sys_ni_syscall	/* old lock syscall holder */
	.long sys_ioctl
	.long sys_fcntl		/* 55 */
	.long sys_ni_syscall	/* old mpx syscall holder */
	.long sys_setpgid
	.long sys_ni_syscall	/* old ulimit syscall holder */
	.long sys_ni_syscall	/* sys_olduname */
	.long sys_umask		/* 60 */
	.long sys_chroot
	.long sys_ustat
	.long sys_dup2
	.long sys_getppid
	.long sys_getpgrp	/* 65 */
	.long sys_setsid
	.long sys_sigaction
	.long sys_sgetmask
	.long sys_ssetmask
	.long sys_setreuid16	/* 70 */
	.long sys_setregid16
	.long sys_sigsuspend
	.long sys_sigpending
	.long sys_sethostname
	.long sys_setrlimit	/* 75 */
	.long sys_old_getrlimit
	.long sys_getrusage
	.long sys_gettimeofday
	.long sys_settimeofday
	.long sys_getgroups16	/* 80 */
	.long sys_setgroups16
	.long sys_ni_syscall	/* sys_oldselect */
	.long sys_symlink
	.long sys_lstat
	.long sys_readlink	/* 85 */
	.long sys_uselib
	.long sys_swapon
	.long sys_reboot
	.long old_readdir
	.long old_mmap		/* 90 */
	.long sys_munmap
	.long sys_truncate
	.long sys_ftruncate
	.long sys_fchmod
	.long sys_fchown16	/* 95 */
	.long sys_getpriority
	.long sys_setpriority
	.long sys_ni_syscall	/* old profil syscall holder */
	.long sys_statfs
	.long sys_fstatfs	/* 100 */
	.long sys_ni_syscall	/* ioperm */
	.long sys_socketcall
	.long sys_syslog
	.long sys_setitimer
	.long sys_getitimer	/* 105 */
	.long sys_newstat
	.long sys_newlstat
	.long sys_newfstat
	.long sys_uname
	.long sys_ni_syscall	/* 110 */ /* iopl */
	.long sys_vhangup
	.long sys_ni_syscall	/* idle */
	.long sys_ni_syscall	/* vm86old */
	.long sys_wait4
	.long sys_swapoff	/* 115 */
	.long sys_sysinfo
	.long sys_ipc
	.long sys_fsync
	.long sys_sigreturn
	.long sys_clone		/* 120 */
	.long sys_setdomainname
	.long sys_newuname
	.long sys_ni_syscall	/* sys_modify_ldt */
	.long sys_adjtimex
	.long sys_mprotect	/* 125 */
	.long sys_sigprocmask
	.long sys_create_module
	.long sys_init_module
	.long sys_delete_module
	.long sys_get_kernel_syms	/* 130 */
	.long sys_quotactl
	.long sys_getpgid
	.long sys_fchdir
	.long sys_bdflush
	.long sys_sysfs		/* 135 */
	.long sys_personality
	.long sys_ni_syscall	/* for afs_syscall */
	.long sys_setfsuid16
	.long sys_setfsgid16
	.long sys_llseek	/* 140 */
	.long sys_getdents
	.long sys_select
	.long sys_flock
	.long sys_msync
	.long sys_readv		/* 145 */
	.long sys_writev
	.long sys_getsid
	.long sys_fdatasync
	.long sys_sysctl
	.long sys_mlock		/* 150 */
	.long sys_munlock
	.long sys_mlockall
	.long sys_munlockall
	.long sys_sched_setparam
	.long sys_sched_getparam   /* 155 */
	.long sys_sched_setscheduler
	.long sys_sched_getscheduler
	.long sys_sched_yield
	.long sys_sched_get_priority_max
	.long sys_sched_get_priority_min  /* 160 */
	.long sys_sched_rr_get_interval
	.long sys_nanosleep
	.long sys_mremap
	.long sys_setresuid16
	.long sys_getresuid16	/* 165 */
	.long sys_ni_syscall	/* vm86 */
	.long sys_query_module
	.long sys_poll
	.long sys_nfsservctl
	.long sys_setresgid16	/* 170 */
	.long sys_getresgid16
	.long sys_prctl
	.long sys_rt_sigreturn
	.long sys_rt_sigaction
	.long sys_rt_sigprocmask	/* 175 */
	.long sys_rt_sigpending
	.long sys_rt_sigtimedwait
	.long sys_rt_sigqueueinfo
	.long sys_rt_sigsuspend
	.long sys_pread		/* 180 */
	.long sys_pwrite
	.long sys_chown16
	.long sys_getcwd
	.long sys_capget
	.long sys_capset	/* 185 */
	.long sys_sigaltstack
	.long sys_sendfile
	.long sys_ni_syscall	/* streams1 */
	.long sys_ni_syscall	/* streams2 */
	.long sys_vfork		/* 190 */
	.long sys_getrlimit
	.long sys_mmap2
	.long sys_truncate64
	.long sys_ftruncate64
	.long sys_stat64	/* 195 */
	.long sys_lstat64
	.long sys_fstat64
	.long sys_lchown
	.long sys_getuid
	.long sys_getgid	/* 200 */
	.long sys_geteuid
	.long sys_getegid
	.long sys_setreuid
	.long sys_setregid
	.long sys_getgroups	/* 205 */
	.long sys_setgroups
	.long sys_fchown
	.long sys_setresuid
	.long sys_getresuid
	.long sys_setresgid	/* 210 */
	.long sys_getresgid
	.long sys_chown
	.long sys_setuid
	.long sys_setgid
	.long sys_setfsuid	/* 215 */
	.long sys_setfsgid
	.long sys_pivot_root
	.long sys_mincore
	.long sys_madvise
	.long sys_getdents64	/* 220 */
	.long sys_fcntl64
	.long sys_gettid
	.long sys_tkill

	/*
	 * NOTE!! This doesn't have to be exact - we just have
	 * to make sure we have _enough_ of the "sys_ni_syscall"
	 * entries. Don't panic if you notice that this hasn't
	 * been shrunk every time we add a new system call.
	 */
	.rept NR_syscalls-221
		.long sys_ni_syscall
	.endr

/* End of entry.S */
