#ifndef TILL_BSP_EXTENSION_H
#define TILL_BSP_EXTENSION_H

#include <rtems.h>
#include <bsp.h>

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
 * NOTE: This routine is slow and temporarily disables interrupts (while installing
 *       and restoring a exception handler). It is intended for probing by driver
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

/* remove a data access breakpoint. Note that this routine only
 * succeeds if the supplied parameters match the installed breakpoint.
 * Exception: 'dataAddr==NULL' will remove _any_ breakpoint.
 */
rtems_status_code
bspExtRemoveDataBreakpoint(void *dataAddr, int mode, BspExtBpntHdlr usrHandler, void *usrArg);


#endif
