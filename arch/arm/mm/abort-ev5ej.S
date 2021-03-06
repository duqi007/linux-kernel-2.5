#include <linux/linkage.h>
#include <asm/assembler.h>
/*
 * Function: v5ej_early_abort
 *
 * Params  : r2 = address of aborted instruction
 *         : r3 = saved SPSR
 *
 * Returns : r0 = address of abort
 *	   : r1 = FSR, bit 8 = write
 *	   : r2-r8 = corrupted
 *	   : r9 = preserved
 *	   : sp = pointer to registers
 *
 * Purpose : obtain information about current aborted instruction.
 * Note: we read user space.  This means we might cause a data
 * abort here if the I-TLB and D-TLB aren't seeing the same
 * picture.  Unfortunately, this does happen.  We live with it.
 */
	.align	5
ENTRY(v5ej_early_abort)
	mrc	p15, 0, r1, c5, c0, 0		@ get FSR
	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
	tst	r3, #PSR_J_BIT
	orrne	r1, r1, #1 << 8			@ always assume write
	bne	1f
	tst	r3, #PSR_T_BIT
	ldrneh	r3, [r2]			@ read aborted thumb instruction
	ldreq	r3, [r2]			@ read aborted ARM instruction
	movne	r3, r3, lsl #(21 - 12)		@ move thumb bit 11 to ARM bit 20
	tst	r3, #1 << 20			@ L = 1 -> write
	orreq	r1, r1, #1 << 8			@ yes.
1:	mov	pc, lr


