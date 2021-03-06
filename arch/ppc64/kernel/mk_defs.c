/*
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines,
 * compile this file to assembler, and then extract the
 * #defines from the assembly-language output.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <stddef.h>
#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/hardirq.h>

#include <asm/Naca.h>
#include <asm/Paca.h>
#include <asm/iSeries/ItLpPaca.h>
#include <asm/iSeries/ItLpQueue.h>
#include <asm/iSeries/HvLpEvent.h>
#include <asm/prom.h>
#include <asm/rtas.h>

#define DEFINE(sym, val) \
	        asm volatile("\n#define\t" #sym "\t%0" : : "i" (val))

int
main(void)
{
	/* thread struct on stack */
	DEFINE(THREAD_SHIFT, THREAD_SHIFT);
	DEFINE(THREAD_SIZE, THREAD_SIZE);
	DEFINE(TI_FLAGS, offsetof(struct thread_info, flags));
	DEFINE(_TIF_32BIT, _TIF_32BIT);

	/* task_struct->thread */
	DEFINE(THREAD, offsetof(struct task_struct, thread));
	DEFINE(LAST_SYSCALL, offsetof(struct thread_struct, last_syscall));
	DEFINE(PT_REGS, offsetof(struct thread_struct, regs));
	DEFINE(THREAD_FPR0, offsetof(struct thread_struct, fpr[0]));
	DEFINE(THREAD_FPSCR, offsetof(struct thread_struct, fpscr));
	DEFINE(KSP, offsetof(struct thread_struct, ksp));

	DEFINE(MM, offsetof(struct task_struct, mm));

	/* Naca */
	DEFINE(DCACHEL1LINESIZE, offsetof(struct Naca, dCacheL1LineSize));
        DEFINE(DCACHEL1LOGLINESIZE, offsetof(struct Naca, dCacheL1LogLineSize));
        DEFINE(DCACHEL1LINESPERPAGE, offsetof(struct Naca, dCacheL1LinesPerPage));
        DEFINE(ICACHEL1LINESIZE, offsetof(struct Naca, iCacheL1LineSize));
        DEFINE(ICACHEL1LOGLINESIZE, offsetof(struct Naca, iCacheL1LogLineSize));
        DEFINE(ICACHEL1LINESPERPAGE, offsetof(struct Naca, iCacheL1LinesPerPage));
	DEFINE(SLBSIZE, offsetof(struct Naca, slb_size));

	/* Paca */
        DEFINE(PACA, offsetof(struct Naca, paca));
        DEFINE(PACA_SIZE, sizeof(struct Paca));
        DEFINE(PACAPACAINDEX, offsetof(struct Paca, xPacaIndex));
        DEFINE(PACAPROCSTART, offsetof(struct Paca, xProcStart));
        DEFINE(PACAKSAVE, offsetof(struct Paca, xKsave));
	DEFINE(PACACURRENT, offsetof(struct Paca, xCurrent));
        DEFINE(PACASAVEDMSR, offsetof(struct Paca, xSavedMsr));
        DEFINE(PACASTABREAL, offsetof(struct Paca, xStab_data.real));
        DEFINE(PACASTABVIRT, offsetof(struct Paca, xStab_data.virt));
	DEFINE(PACASTABRR, offsetof(struct Paca, xStab_data.next_round_robin));
        DEFINE(PACAR1, offsetof(struct Paca, xR1));
        DEFINE(PACALPQUEUE, offsetof(struct Paca, lpQueuePtr));
	DEFINE(PACATOC, offsetof(struct Paca, xTOC));
	DEFINE(PACAEXCSP, offsetof(struct Paca, exception_sp));
	DEFINE(PACAHRDWINTSTACK, offsetof(struct Paca, xHrdIntStack));
	DEFINE(PACAPROCENABLED, offsetof(struct Paca, xProcEnabled));
	DEFINE(PACAHRDWINTCOUNT, offsetof(struct Paca, xHrdIntCount));
	DEFINE(PACADEFAULTDECR, offsetof(struct Paca, default_decr));
	DEFINE(PACAPROFENABLED, offsetof(struct Paca, prof_enabled));
	DEFINE(PACAPROFLEN, offsetof(struct Paca, prof_len));
	DEFINE(PACAPROFSHIFT, offsetof(struct Paca, prof_shift));
	DEFINE(PACAPROFBUFFER, offsetof(struct Paca, prof_buffer));
	DEFINE(PACAPROFSTEXT, offsetof(struct Paca, prof_stext));
	DEFINE(PACALPPACA, offsetof(struct Paca, xLpPaca));
        DEFINE(LPPACA, offsetof(struct Paca, xLpPaca));
        DEFINE(PACAREGSAV, offsetof(struct Paca, xRegSav));
        DEFINE(PACAEXC, offsetof(struct Paca, exception_stack));
        DEFINE(PACAGUARD, offsetof(struct Paca, guard));
        DEFINE(LPPACASRR0, offsetof(struct ItLpPaca, xSavedSrr0));
        DEFINE(LPPACASRR1, offsetof(struct ItLpPaca, xSavedSrr1));
	DEFINE(LPPACAANYINT, offsetof(struct ItLpPaca, xIntDword.xAnyInt));
	DEFINE(LPPACADECRINT, offsetof(struct ItLpPaca, xIntDword.xFields.xDecrInt));
        DEFINE(LPQCUREVENTPTR, offsetof(struct ItLpQueue, xSlicCurEventPtr));
        DEFINE(LPQOVERFLOW, offsetof(struct ItLpQueue, xPlicOverflowIntPending));
        DEFINE(LPEVENTFLAGS, offsetof(struct HvLpEvent, xFlags));
	DEFINE(PROMENTRY, offsetof(struct prom_t, entry));

	/* RTAS */
	DEFINE(RTASBASE, offsetof(struct rtas_t, base));
	DEFINE(RTASENTRY, offsetof(struct rtas_t, entry));
	DEFINE(RTASSIZE, offsetof(struct rtas_t, size));

	/* Interrupt register frame */
	DEFINE(STACK_FRAME_OVERHEAD, STACK_FRAME_OVERHEAD);

	DEFINE(SWITCH_FRAME_SIZE, STACK_FRAME_OVERHEAD + sizeof(struct pt_regs));

	/* 288 = # of volatile regs, int & fp, for leaf routines */
	/* which do not stack a frame.  See the PPC64 ABI.       */
	DEFINE(INT_FRAME_SIZE, STACK_FRAME_OVERHEAD + sizeof(struct pt_regs) + 288);
	/* Create extra stack space for SRR0 and SRR1 when calling prom/rtas. */
	DEFINE(PROM_FRAME_SIZE, STACK_FRAME_OVERHEAD + sizeof(struct pt_regs) + 16);
	DEFINE(RTAS_FRAME_SIZE, STACK_FRAME_OVERHEAD + sizeof(struct pt_regs) + 16);
	DEFINE(GPR0, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[0]));
	DEFINE(GPR1, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[1]));
	DEFINE(GPR2, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[2]));
	DEFINE(GPR3, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[3]));
	DEFINE(GPR4, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[4]));
	DEFINE(GPR5, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[5]));
	DEFINE(GPR6, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[6]));
	DEFINE(GPR7, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[7]));
	DEFINE(GPR8, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[8]));
	DEFINE(GPR9, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[9]));
	DEFINE(GPR20, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[20]));
	DEFINE(GPR21, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[21]));
	DEFINE(GPR22, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[22]));
	DEFINE(GPR23, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, gpr[23]));
	/*
	 * Note: these symbols include _ because they overlap with special
	 * register names
	 */
	DEFINE(_NIP, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, nip));
	DEFINE(_MSR, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, msr));
	DEFINE(_CTR, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, ctr));
	DEFINE(_LINK, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, link));
	DEFINE(_CCR, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, ccr));
	DEFINE(_XER, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, xer));
	DEFINE(_DAR, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, dar));
	DEFINE(_DSISR, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, dsisr));
	DEFINE(ORIG_GPR3, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, orig_gpr3));
	DEFINE(RESULT, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, result));
	DEFINE(TRAP, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, trap));
	DEFINE(SOFTE, STACK_FRAME_OVERHEAD+offsetof(struct pt_regs, softe));

	/* These _only_ to be used with {PROM,RTAS}_FRAME_SIZE!!! */
	DEFINE(_SRR0, STACK_FRAME_OVERHEAD+sizeof(struct pt_regs));
	DEFINE(_SRR1, STACK_FRAME_OVERHEAD+sizeof(struct pt_regs)+8);

	DEFINE(CLONE_VM, CLONE_VM);

	return 0;
}
