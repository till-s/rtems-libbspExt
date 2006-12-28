#ifndef TILL_BSP_EXTENSION_H
#define TILL_BSP_EXTENSION_H
/* $Id$ */

/* RTEMS-PowerPC BSP extension library (address probing) */

/* Author: Till Straumann <strauman@@slac.stanford.edu>  */

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

#include <rtems.h>
#include <bsp.h>

/* Macro to detect RTEMS version */
#include <rtems/system.h>

#define RTEMS_ISMINVERSION(ma,mi,re) \
	(    __RTEMS_MAJOR__  > (ma)	\
	 || (__RTEMS_MAJOR__ == (ma) && __RTEMS_MINOR__  > (mi))	\
	 || (__RTEMS_MAJOR__ == (ma) && __RTEMS_MINOR__ == (mi) && __RTEMS_REVISION__ >= (re)) \
    )


/* initialize the bsp extensions */
rtems_status_code
bspExtInit(void);

/* probe a memory address:
 *
 * addr : address to probe
 * write: writes *pval to address if !=0, reads from address to *pval,
 *        otherwise.
 * size : size of the probe; must be 1 2 or 4
 * pval : pointer to value that is written or read
 *
 * RETURNS : RTEMS_SUCCESSFUL		on probe success
 *           RTEMS_INVALID_ADDRESS 	probed address not mapped/accessible
 *           other			error
 *
 * NOTE: This routine is VERY slow and temporarily disables interrupts (while installing
 *       and restoring a exception handler and during execution of the handler
 *       itself, which can take a LONG time!!!).
 *
 *       It is intended for probing by driver
 *       initialization code and NOT for ordinary reads/writes.
 */

rtems_status_code
bspExtMemProbe(void *addr, int write, int size, void *pval);


/* user handler for a DABR (data access breakpoint)
 *
 * ARGUMENTS:
 *  - 'before':  the handler is invoked twice, once before and again
 *               after the faulting instruction has completed.
 *  - 'frame' :  the register context.
 *
 * NOTE: this handler is run in EXCEPTION CONTEXT like any exception
 *       handler!
 *
 * RETURN VALUE:
 *  if the user handler returns a nonzero value, the BSP will panic
 *  (i.e. print context info and halt)
 */
typedef int (*BspExtBpntHdlr)(int before, BSP_Exception_frame *frame, void *usrArg);

/* install a data access breakpoint to 'dataAddr'
 *
 * ARGUMENTS:
 *  - 'dataAddr': an exception will be raised by the hardware upon a
 *                data access to this address (note that the PPC only
 *                supports 8byte granularity)
 *  - 'mode'    : break on read, write, or both. The BT flag is checked
 *                against the MSR[DR} bit...
 *
 * NOTE: only 1 DABR is supported.
 */

/* mode can be a bitwise or of */
#define DABR_RD         (1<<0)	/* break on (data) read access to DABR address */
#define DABR_WR         (1<<1)	/* break on (data) write access to DABR address */
#define DABR_BT         (1<<2)	/* breakpoint address translation (breaks only if this matches MSR[DR]) */
#define DABR_MODE_COARSE (1<<3)	/* enable 'coarse' mode, i.e. catch accesses anywhere
				 * in (addr)&~7 ... (addr)|7
				 */

rtems_status_code
bspExtInstallDataBreakpoint(void *dataAddr, int mode, BspExtBpntHdlr usrHandler, void *usrArg);

/* install an instruction access breakpoint to 'addr'
 *
 * ARGUMENTS:
 *  - 'addr': an exception will be raised by the hardware upon an
 *            instruction access to this address.
 *
 * NOTE: only 1 IABR is supported.
 */

rtems_status_code
bspExtInstallBreakpoint(void *addr, BspExtBpntHdlr usrHandler, void *usrArg);


/* remove a data access breakpoint. Note that this routine only
 * succeeds if the supplied parameter matches the installed breakpoint.
 * Exception: 'dataAddr==NULL' will remove _any_ (data access) breakpoint.
 */
rtems_status_code
bspExtRemoveDataBreakpoint(void *dataAddr);

/* remove an instruction access breakpoint. Note that this routine only
 * succeeds if the supplied parameter matches the installed breakpoint.
 * Exception: 'addr==NULL' will remove _any_ (instruction) breakpoint.
 */
rtems_status_code
bspExtRemoveBreakpoint(void *addr);


/* Silence warnings by setting this variable to zero:
 *
 *    0 -- silent
 *    1 -- warnings
 *    2 -- info
 * >= 3 -- debugging
 * 
 */ 
extern int bspExtVerbosity;

/* Install an ISR ( void (*isr)(void *uarg) ) to an interrupt line
 * ('name' in the RTEMS BSP jargon; these can be found in <bsp/irq.h>
 * E.g. the 'name' of a PCI interrupt line as read from PCI configuration
 * space has to be offset by BSP_PCI_IRQ_LOWEST_OFFSET on some BSPs.)
 *
 * Unless the BSPEXT_ISR_NONSHARED flag is set, ISRs are expected
 * to support interrupt sharing.
 *
 * IMPLEMENTATION NOTE:
 *  This is implemented by installing a wrapper onto the generic
 *  RTEMS/BSP API. The wrapper then dispatches all registered ISRs
 *  and provides them with the user arg etc.
 * 
 * RETURNS: 0 on success, nonzero on error.
 */
#define BSPEXT_ISR_NONSHARED 1
int
bspExtInstallSharedISR(int name, void (*isr)(void *uarg), void * uarg, int flags);

/* Remove an ISR. Both, the 'isr' and 'uarg' parameters must match
 * a registered ISR on the specified line ('name').
 *
 * IMPLEMENTATION NOTE:
 * 
 * RETURNS: 0 on success, nonzero on error.
 */
int
bspExtRemoveSharedISR(int name, void (*isr)(void *uarg), void *uarg);

/* Lock unlock critical sections of the libary */
void bspExtLock();
void bspExtUnlock();

/* Exception Handlers */

/* An exception handler
 *
 * RETURNS 0 if the exception was 'caught', nonzero otherwise.
 *
 * The library traverses the list of registered handlers
 * executing them. If a handler returns 0, list traversion
 * is aborted and control returned.
 * If none of the registered handlers catches the exception,
 * control is passed on to the BSP's original handler.
 */
int BspExtExceptionHandler(BSP_Exception_frame *f, void *usrData);

typedef int (*BspExtEHandler)(BSP_Exception_frame *, void *);

/* NOTE: These routines temporarily disable interrupts */

/*
 * Register an exception handler at the head (where > 0) or tail
 * (where <=0).
 *
 * RETURNS: 0 on success, -1 on failure (handler already installed
 *          or no memory).
 *
 */
int bspExtInstallEHandler(BspExtEHandler h, void *usrData, int where);

/* Remove an exception handler
 *
 * RETURNS: 0 on success, nonzero if handler/usrData pair not found
 */
int bspExtRemoveEHandler(BspExtEHandler h, void *usrData);

#endif
