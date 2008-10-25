/* $Id$ */

/* rudimentary and crude DABR hardware breakpoint support for Powerpc-RTEMS */

/* 
 * Authorship
 * ----------
 * This software (libbspExt - extensions to ppc and some other BSPs)
 * was created by
 *
 *    Till Straumann <strauman@slac.stanford.edu>, 2002-2008,
 * 	  Stanford Linear Accelerator Center, Stanford University.
 *
 * Acknowledgement of sponsorship
 * ------------------------------
 * This software was produced by
 *     the Stanford Linear Accelerator Center, Stanford University,
 * 	   under Contract DE-AC03-76SFO0515 with the Department of Energy.
 * 
 * Government disclaimer of liability
 * ----------------------------------
 * Neither the United States nor the United States Department of Energy,
 * nor any of their employees, makes any warranty, express or implied, or
 * assumes any legal liability or responsibility for the accuracy,
 * completeness, or usefulness of any data, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately owned
 * rights.
 * 
 * Stanford disclaimer of liability
 * --------------------------------
 * Stanford University makes no representations or warranties, express or
 * implied, nor assumes any liability for the use of this software.
 * 
 * Stanford disclaimer of copyright
 * --------------------------------
 * Stanford University, owner of the copyright, hereby disclaims its
 * copyright and all other rights in this software.  Hence, anyone may
 * freely use it for any purpose without restriction.  
 * 
 * Maintenance of notices
 * ----------------------
 * In the interest of clarity regarding the origin and status of this
 * SLAC software, this and all the preceding Stanford University notices
 * are to remain affixed to any copy or derivative of this software made
 * or distributed by the recipient and are to be affixed to any copy of
 * software made or distributed by the recipient that contains a copy or
 * derivative of this software.
 * 
 * ------------------ SLAC Software Notices, Set 4 OTT.002a, 2004 FEB 03
 */ 

#include "bspExt.h"

#define DBPNT 0
#define IBPNT 1

#define R_DABR	1013
#define R_IABR	1010

#define IABR_BE	2
#define IABR_TE 1

#define DEBUG
#ifdef DEBUG
#define STATIC
unsigned getDabr()
{
unsigned rval;
	asm volatile("mfspr %0, %1":"=r"(rval):"i"(R_DABR));
	return rval;
}

unsigned getIabr()
{
unsigned rval;
	asm volatile("mfspr %0, %1":"=r"(rval):"i"(R_IABR));
	return rval;
}
#else
#define STATIC static
#endif

STATIC struct bpnt_ {
	void			*addr;
	int				mode;
	BspExtBpntHdlr	usrHdlr;	
	void			*usrArg;
	unsigned long	saved_msr;
} BPNTS[2] = { { 0, }, };

#define DABR_FLGS	7
#define IABR_FLGS	3

__asm__(
	".LOCAL my_syscall	\n"
	"my_syscall: sc\n"
);

extern void my_syscall(void);

static rtems_status_code
doinstall(int type, void *addr, int mode, BspExtBpntHdlr usrHandler, void *usrArg)
{
rtems_status_code		rval=RTEMS_INVALID_ADDRESS;
unsigned long			mask;
unsigned long			reg;

	mask = DBPNT == type ? DABR_FLGS : IABR_FLGS;
	if ( DBPNT != type )
		type = IBPNT;

	/* combine aligned address and mode */
	reg = ( ((unsigned long)addr) & ~mask ) | ( mode & mask );

	bspExtLock();

	if (!BPNTS[type].addr) {
		BPNTS[type].addr=addr;
		BPNTS[type].usrHdlr=usrHandler;
		BPNTS[type].mode=mode;
		BPNTS[type].usrArg=usrArg;
		/* setup DABR */
		if ( DBPNT == type )
			__asm__ __volatile__("mtspr %1, %0"::"r"(reg),"i"(R_DABR));
		else
			__asm__ __volatile__("mtspr %1, %0"::"r"(reg),"i"(R_IABR));
		rval=RTEMS_SUCCESSFUL;
	}
	bspExtUnlock();
	return rval;
}

rtems_status_code
bspExtInstallDataBreakpoint(void *dataAddr, int mode, BspExtBpntHdlr usrHandler, void *usrArg)
{
	/* lazy init */
	bspExtInit();

#warning "FIXME: add run-time check for CPU type"

	if (!(mode & (DABR_WR|DABR_RD)))
		return RTEMS_INVALID_NUMBER;	/* invalid mode */

	return doinstall(DBPNT, dataAddr, mode, usrHandler, usrArg);
}

rtems_status_code
bspExtInstallBreakpoint(void *addr, BspExtBpntHdlr usrHandler, void *usrArg)
{
	/* lazy init */
	bspExtInit();

	return doinstall(IBPNT, addr, IABR_FLGS, usrHandler, usrArg);
}

static rtems_status_code
douninstall(int type, void *addr)
{
rtems_status_code	rval=RTEMS_INVALID_ADDRESS;

	/* lazy init */
	bspExtInit();
	
	if ( DBPNT != type )
		type = IBPNT;

	bspExtLock();

	if (0==addr || (BPNTS[type].addr==addr) ) {
		BPNTS[type].addr=0;
		/* setup DABR/IABR */
		if ( DBPNT == type )
			__asm__ __volatile__("mtspr %1, %0"::"r"(0),"i"(R_DABR));
		else
			__asm__ __volatile__("mtspr %1, %0"::"r"(0),"i"(R_IABR));
		rval=RTEMS_SUCCESSFUL;
	}

	bspExtUnlock();

	return rval;
}


rtems_status_code
bspExtRemoveDataBreakpoint(void *dataAddr) 
{
	return douninstall(DBPNT, dataAddr);
}

rtems_status_code
bspExtRemoveBreakpoint(void *addr) 
{
	return douninstall(IBPNT, addr);
}

/* 'type' is bit 0, phase is bit 1 */
#define CAUSE_DABR_PHASE1	0
#define CAUSE_DABR_PHASE2	2
#define CAUSE_IABR_PHASE1	1
#define CAUSE_IABR_PHASE2	3

#define PHASE2			2
#define PHASE(cause)	((cause)&2)
#define TYPE(cause)		((cause)&1)


int
_bspExtCatchBreakpoint(BSP_Exception_frame *fp)
{
static int		traceInProgress = -1;
int				rval=0;
unsigned long	tmp =0;	/* silence compiler warnings */
int				cause = -1;

/* check for catch condition */
	if ( 3==fp->_EXC_number && 
         (BPNTS[DBPNT].mode & DABR_MODE_COARSE ?
           !(((long)BPNTS[DBPNT].addr ^ (long)fp->EXC_DAR) & ~DABR_FLGS) :
           (long)BPNTS[DBPNT].addr == (long)fp->EXC_DAR
         ) )
		cause = CAUSE_DABR_PHASE1;
	else if ( 0x13 == fp->_EXC_number &&
		      fp->EXC_SRR0 == ((unsigned long)BPNTS[IBPNT].addr & ~IABR_FLGS) )
		cause = CAUSE_IABR_PHASE1;
	else if ( 0x0d == fp->_EXC_number && traceInProgress >= 0 ) {
		cause = traceInProgress | PHASE2;
		traceInProgress = -1;
	}

	if ( -1 == cause )
		return rval;

	if ( ! (PHASE2 & cause) ) {
		/* temporarily disable breakpoint */
		if ( DBPNT == TYPE(cause) ) {
			__asm__ __volatile__("mfspr %0, %2; andc %0, %0, %1; mtspr %2, %0; isync"::"r"(tmp),"r"(DABR_WR|DABR_RD),"i"(R_DABR));
		} else {
			__asm__ __volatile__("mfspr %0, %2; andc %0, %0, %1; mtspr %2, %0; isync"::"r"(tmp),"r"(IABR_BE),"i"(R_IABR));
		}

		/* save MSR; enable single step mode */
		BPNTS[TYPE(cause)].saved_msr = fp->EXC_SRR1;
		fp->EXC_SRR1 |= MSR_SE;	
		/* disable interrupts while stepping over the breakpoint */
		fp->EXC_SRR1 &= ~MSR_EE;

		/* call the user handler */
		rval=(0==BPNTS[TYPE(cause)].usrHdlr ||
		         BPNTS[TYPE(cause)].usrHdlr(1,fp,BPNTS[TYPE(cause)].usrArg)) ? -1 : 1;
		traceInProgress = cause;
	} else {
		/* must be phase 2 */

		/* restore MSR */
		fp->EXC_SRR1 = BPNTS[TYPE(cause)].saved_msr;

		rval=(0==BPNTS[TYPE(cause)].usrHdlr ||
		         BPNTS[TYPE(cause)].usrHdlr(0,fp,BPNTS[TYPE(cause)].usrArg)) ? -1 : 2;

		/* reinstall the DABR _after_ calling their handler - it may reference
	 	 * the target memory location...
	 	 */
		if ( DBPNT == TYPE(cause) ) {
			__asm__ __volatile__("mfspr %0, %2; or %0, %0, %1; mtspr %2, %0; isync"::"r"(tmp),"r"(BPNTS[DBPNT].mode&DABR_FLGS),"i"(R_DABR));
		} else {
			__asm__ __volatile__("mfspr %0, %2; or %0, %0, %1; mtspr %2, %0; isync"::"r"(tmp),"r"(IABR_FLGS),"i"(R_IABR));
		}
	}
return rval;
}
