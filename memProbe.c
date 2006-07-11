/* $Id$ */

/* Address Probing for Powerpc - RTEMS
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>
 *
 * S. Kate Feng <feng1@bnl.gov> ported it to the MVME5500.(4/2004)
 *
 */

/*
 * Copyright 2002,2003, Stanford University and
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bsp.h>
#include <rtems/bspIo.h>
#include <assert.h>

#include <libcpu/spr.h>

#define SRR1_TEA_EXC    (1<<(31-13))
#define SRR1_MCP_EXC    (1<<(31-12))

#if defined(mpc7455) || defined(mpc7450)
SPR_RW(HID1)
#define _read_HIDx    _read_HID1
#define _write_HIDx   _write_HID1
#elif defined(mpc7400) || defined(mpc750) || defined(mpc604) || defined(mpc603)
SPR_RW(HID0)
#define _read_HIDx    _read_HID0
#define _write_HIDx   _write_HID0
#else
#warning "Hmmm what CPU flavor are you using -- is EMPC in HID0 or HID1 ?"
#warning "Disabling MCP interrupt support -- must poll with interrupts disabled"
#endif

typedef struct EH_NodeRec_ {
	BspExtEHandler	   h;
	void			   *d;
	struct EH_NodeRec_ *n;
} EH_NodeRec, *EH_Node;

static EH_Node anchor = 0;

static exception_handler_t	origHandler=0;

/* lowlevel memory probing routines conforming to
 * the PPC SVR4 ABI.
 * Note: r3, i.e. the return value is modified
 *       (implicitely) by the exeption handler
 *       the routines return the fault address
 *       if an exception was detected, hence
 *       the first argument should be set e.g.
 *       to 0 when calling the probe routine.
 */

typedef void * (*MemProber)(void *rval, void *from, void *to);

void * memProbeByte(void *rval, void *from, void *to);
void * memProbeShort(void *rval, void *from, void *to);
void * memProbeLong(void *rval, void *from, void *to);
void * memProbeEnd(void);


__asm__(
	"	.text\n"
	"	.GLOBAL memProbeByte, memProbeShort, memProbeLong, memProbeEnd\n"
	"memProbeByte:		\n"
	"	lbz %r4, 0(%r4) \n"
	"	stb %r4, 0(%r5) \n"
	"	b	1f			\n"
	"memProbeShort:		\n"
	"	lhz %r4, 0(%r4) \n"
	"	sth %r4, 0(%r5) \n"
	"	b	1f			\n"
	"memProbeLong:		\n"
	"	lwz %r4, 0(%r4) \n"
	"	stw %r4, 0(%r5) \n"
	"1:	sync            \n"
	"	nop             \n"	/* could take some time until MCP is asserted */
	"	nop             \n"
	"	nop             \n"
	"	nop             \n"
	"	nop             \n"
	"	nop             \n"
	"	nop             \n"
	"	nop             \n"
	"	sync            \n"
	"memProbeEnd:       \n"
	"	blr             \n"
);

extern int
_bspExtCatchBreakpoint(BSP_Exception_frame *fp);

static void
bspExtExceptionHandler(BSP_Exception_frame* excPtr)
{
void		*nip = (void*)excPtr->EXC_SRR0;
int			caughtBr;

	/* is this a memProbe? */
	if (nip>=(void*)memProbeByte && nip<(void*)memProbeEnd) {
		/* well, we assume that the branch
		 * instructions between memProbeByte and memProbeLong
		 * are safe and dont fault...
		 */
		/* flag the fault */
		excPtr->GPR3=(unsigned)nip;
		excPtr->EXC_SRR0 +=4;	/* skip the faulting instruction */
		/* NOTE DAR is not valid at this point because some boards
    	 * raise a machine check which doesn't set DAR...
		 */
 		/* clear MCP condition */
 		if ( (SRR1_TEA_EXC|SRR1_MCP_EXC) & excPtr->EXC_SRR1 ) {
			unsigned status;
 			status = _BSP_clear_hostbridge_errors(1,1);
			if ( bspExtVerbosity > 1 ) {
				printk("Machine check [%s]; host bridge status 0x%08x\n",
						excPtr->EXC_SRR1 & SRR1_TEA_EXC ? "TEA" : "MCP",
						status);
			}
 		}
		return;
	} else if ((caughtBr=_bspExtCatchBreakpoint(excPtr))) {
		switch (caughtBr) {
			default: /* should never get here */
			case -1: /* usr handler wants us to panic */
				break;

			case  1: /* phase 1; dabr deinstalled, complete data access */
			case  2: /* phase 2; stopped after completing faulting instruction */
				return;
		}
	}
	/* Work the list of registered handlers */
	{
	EH_Node p;
	for ( p = anchor; p; p=p->n ) {
		if ( 0 == (*p->h)(excPtr, p->d) )
			return;
	}
	}

	/* resort to BSP handler */
	origHandler(excPtr);
}

static int useMcp = 0;

void
_bspExtMemProbeInit(void)
{
rtems_interrupt_level	level;

#ifdef _write_HIDx
	/* try to clear pending errors and enable
	 * MCP in HID0 on success
	 */

	/* clear pending errors */
	_BSP_clear_hostbridge_errors(0,1);
	/* see if there are still errors pending */
	if ( _BSP_clear_hostbridge_errors(0,1) ) {
		fprintf(stderr,"Warning: unable to clear pending hostbridge errors; leaving MCP disabled\n");
		fprintf(stderr,"         proper operation of address probing not guaranteed\n");
	} else {
	     /* enable MCP at the hostbridge */
	     if (_BSP_clear_hostbridge_errors(1,1) !=-1) {
			useMcp = 1;
			_write_HIDx( _read_HIDx() | HID0_EMCP );/* enable MCP */
		 }
	}
#else
	fprintf(stderr,"libbspExt - Warning: it seems that MCP support is not available or not\n");
	fprintf(stderr,"                     implemented on your board. Address probing must be\n");
	fprintf(stderr,"                     performed in polling mode with interrupts disabled\n");
#endif

	/* switch exception handlers */
	rtems_interrupt_disable(level);
	/* install an exception handler to catch
	 * protection violations
	 */
	origHandler=globalExceptHdl;
	globalExceptHdl = bspExtExceptionHandler;

	/* make sure handler is swapped */
	__asm__ __volatile__ ("sync");
	rtems_interrupt_enable(level);
}


rtems_status_code
bspExtMemProbe(void *addr, int write, int size, void *pval)
{
rtems_status_code		rval=RTEMS_SUCCESSFUL;
rtems_interrupt_level	flags=0;
unsigned long			buf;
MemProber				probe;
void					*faultAddr;

	/* lazy init (not strictly thread safe!) */
	if (!origHandler) bspExtInit();

	if (bspExtExceptionHandler!=globalExceptHdl) {
		fprintf(stderr,"bspExtMemProbe: Someone changed the exception handler - refuse to probe\n");
		return -1;
	}

	if (bspExtVerbosity) {
		fprintf(stderr,"Warning: bspExtMemProbe kills real-time performance.\n");
		if ( !useMcp ) {
		    fprintf(stderr,"         Your BSP does or can not use MCP exceptions - we\n");
		    fprintf(stderr,"         must probe with INTERRUPTS DISABLED !!!\n");
		}
	    fprintf(stderr,"         use only during driver initialization\n\n");
		fprintf(stderr,"         clear the 'bspExtVerbosity' variable to silence this warning\n");
	}

	/* validate arguments and compute jump table index */
	switch (size) {
		case sizeof(char):  probe=memProbeByte; break;
		case sizeof(short): probe=memProbeShort; break;
		case sizeof(long):  probe=memProbeLong; break;
		default: return RTEMS_INVALID_SIZE;
	}

	/* use a buffer to make sure we don't end up
	 * probing 'pval'...
	 */
	if (write && pval)
		memcpy(&buf, pval, size);

	if ( ! useMcp ) {
		/* MCP exceptions are not enabled; use
		 * exclusive access to host bridge status
		 * - data access exceptions still work...
		 */
rtems_interrupt_disable(flags);
		_BSP_clear_hostbridge_errors(0,1);
	}

	if (write) {
		if ((faultAddr=probe(0,&buf,addr)))
			rval=RTEMS_INVALID_ADDRESS;
	} else {
		if ((faultAddr=probe(0,addr,&buf))) {
			rval=RTEMS_INVALID_ADDRESS;
			/* pass them the fault address back */
		}
	}

	if ( ! useMcp ) {
		/* MCP exceptions are not enabled; use
		 * exclusive access to host bridge status
		 */
		if (!faultAddr)
			rval = _BSP_clear_hostbridge_errors(0,1);
rtems_interrupt_enable(flags);
	}

	if (!write && pval) {
		memcpy(pval, faultAddr ? (void*)&faultAddr : &buf, size);
	}
	return rval;
}

/*
 * Register an exception handler at the head (where > 0) or tail
 * (where <=0).
 *
 * RETURNS: 0 on success, -1 on failure (handler already installed
 *          or no memory).
 *
 */
int
bspExtInstallEHandler(BspExtEHandler h, void *usrData, int where)
{
EH_Node       n=0, *pp;
unsigned long flags;
int           rval = -1;

	if ( !(n=malloc(sizeof(*n))) )
		return -1;

bspExtLock();
	for ( pp=&anchor; *pp; pp=&(*pp)->n ) {
		if ( (*pp)->h == h && (*pp)->d == usrData )
			goto bail;
	}
	/* pp now points to the tail */
	n->h = h;
	n->d = usrData;
	if ( where ) {
		/* reset to head */
		pp = &anchor;
	}

	rtems_interrupt_disable(flags);
		n->n = *pp;
		*pp  = n;
	rtems_interrupt_enable(flags);

	n    = 0;
	rval = 0;

bail:
bspExtUnlock();
	free(n);
	return rval;
}

/* Remove an exception handler
 *
 * RETURNS: 0 on success, nonzero if handler/usrData pair not found
 */
int bspExtRemoveEHandler(BspExtEHandler h, void *usrData)
{
EH_Node       p = 0, *pp;
unsigned long flags;

	bspExtLock();
	for ( pp = &anchor; *pp; pp = &(*pp)->n ) {
		if ( (*pp)->h == h && (*pp)->d == usrData ) {
			rtems_interrupt_disable(flags);
			p = *pp;
			*pp = p->n;
			rtems_interrupt_enable(flags);
			break;
		}
	}
	bspExtUnlock();

	free(p);

	return 0 == p;
}


