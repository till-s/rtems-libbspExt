/* $Id$ */

/* rudimentary and crude DABR hardware breakpoint support for Powerpc-RTEMS */

/* Author: Till Straumann <strauman@slac.stanford.edu>, 2001/2002/2003 */

/*
 * Copyright 2001,2002,2003, Stanford University and
 * 		Till Straumann <strauman@@slac.stanford.edu>
 * 
 * Stanford Notice
 * ***************
 * 
 * Acknowledgement of sponsorship
 * * * * * * * * * * * * * * * * *
 * This software was produced by the Stanford Linear Accelerator Center,
 * Stanford University, under Contract DE-AC03-76SFO0515 with the Department
 * of Energy.
 * 
 * Government disclaimer of liability
 * - - - - - - - - - - - - - - - - -
 * Neither the United States nor the United States Department of Energy,
 * nor any of their employees, makes any warranty, express or implied,
 * or assumes any legal liability or responsibility for the accuracy,
 * completeness, or usefulness of any data, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately
 * owned rights.
 * 
 * Stanford disclaimer of liability
 * - - - - - - - - - - - - - - - - -
 * Stanford University makes no representations or warranties, express or
 * implied, nor assumes any liability for the use of this software.
 * 
 * This product is subject to the EPICS open license
 * - - - - - - - - - - - - - - - - - - - - - - - - - 
 * Consult the LICENSE file or http://www.aps.anl.gov/epics/license/open.php
 * for more information.
 * 
 * Maintenance of notice
 * - - - - - - - - - - -
 * In the interest of clarity regarding the origin and status of this
 * software, Stanford University requests that any recipient of it maintain
 * this notice affixed to any distribution by the recipient that contains a
 * copy or derivative of this software.
 */

#include "bspExt.h"

static void		*BPNT=0;
static BspExtBpntHdlr	BPNT_usrHdlr;
static int   		BPNT_mode=0;
static void		*BPNT_usrArg=0;

#define SC_OPCODE 	0x44000002	/* sc instruction */

#define DABR_FLGS	7


__asm__(
	".LOCAL my_syscall	\n"
	"my_syscall: sc\n"
);

static void my_syscall(void);

rtems_status_code
bspExtInstallDataBreakpoint(void *dataAddr, int mode, BspExtBpntHdlr usrHandler, void *usrArg)
{
rtems_status_code	rval=RTEMS_INVALID_ADDRESS;
rtems_interrupt_level	level;
unsigned long		dabr=(unsigned long)dataAddr;

	/* lazy init */
	bspExtInit();

	if (!(mode & (DABR_WR|DABR_RD)))
		return RTEMS_INVALID_NUMBER;	/* invalid mode */

	/* combine aligned address and mode */
	dabr=(dabr&~DABR_FLGS)|(mode&DABR_FLGS);

	rtems_interrupt_disable(level);
	if (!BPNT) {
		BPNT=dataAddr;
		BPNT_usrHdlr=usrHandler;
		BPNT_mode=mode;
		BPNT_usrArg=usrArg;
		/* setup DABR */
		__asm__ __volatile__("mtspr 1013, %0"::"r"(dabr));
		rval=RTEMS_SUCCESSFUL;
	}
	rtems_interrupt_enable(level);

	return rval;
}

rtems_status_code
bspExtRemoveDataBreakpoint(void *dataAddr, int mode, BspExtBpntHdlr usrHandler, void *usrArg)
{
rtems_status_code	rval=RTEMS_INVALID_ADDRESS;
rtems_interrupt_level	level;

	/* lazy init */
	bspExtInit();

	rtems_interrupt_disable(level);
	if (0==dataAddr ||
            (BPNT==dataAddr && BPNT_mode==mode && BPNT_usrHdlr==usrHandler && BPNT_usrArg == usrArg)) {
		BPNT=0;
		/* setup DABR */
		__asm__ __volatile__("mtspr 1013, %0"::"r"(0));
		rval=RTEMS_SUCCESSFUL;
	}
	rtems_interrupt_enable(level);

	return rval;
}

int
_bspExtCatchDabr(BSP_Exception_frame *fp)
{
static unsigned long	saved_instr=0xdeadbeef;
int			rval=0;
unsigned long		tmp=0;
if ( 3==fp->_EXC_number && 
      (BPNT_mode & DABR_MODE_COARSE ?
        !(((long)BPNT ^ (long)fp->EXC_DAR) & ~DABR_FLGS) :
        (long)BPNT == (long)fp->EXC_DAR
      )
   ) {
	/* temporarily disable dabr */
	__asm__ __volatile__("mfspr %0, 1013; andc %0, %0, %1; mtspr 1013, %0"::"r"(tmp),"r"(DABR_WR|DABR_RD));

	/* save next instruction */
	saved_instr = *(unsigned long *)(fp->EXC_SRR0+4);

	/* install syscall opcode */
	*(unsigned long*)(fp->EXC_SRR0+4)=*(unsigned long*)&my_syscall;

	/* force cache write and invalidate instruction cache */
	__asm__ __volatile__("dcbst 0, %0; icbi 0, %0; sync; isync;"::"r"(fp->EXC_SRR0+4));

	/* call the user handler */
	rval=(0==BPNT_usrHdlr || BPNT_usrHdlr(1,fp,BPNT_usrArg)) ? -1 : 1;

} else if (12==fp->_EXC_number && 0xdeadbeef!=saved_instr) {
	/* point back to where we stored 'sc' */
	fp->EXC_SRR0-=4;

	/* restore breakpoint instruction */
	*(unsigned long*)fp->EXC_SRR0=saved_instr; saved_instr=0xdeadbeef;

	/* write out dcache, invalidate icache */
	__asm__ __volatile__("dcbst 0, %0; icbi 0, %0; sync; isync;"::"r"(fp->EXC_SRR0));

	rval=(0==BPNT_usrHdlr || BPNT_usrHdlr(0,fp,BPNT_usrArg)) ? -1 : 2;

	/* reinstall the DABR _after_ calling their handler - it may reference
	 * the target memory location...
	 */
	__asm__ __volatile__("mfspr %0, 1013; or %0, %0, %1; mtspr 1013, %0"::"r"(tmp),"r"(BPNT_mode&DABR_FLGS));
}
return rval;
}
