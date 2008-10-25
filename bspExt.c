/* $Id$ */

/* initialization of libbspExt */

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

#include <stdio.h>
#include <assert.h>
#include <bsp.h>

static rtems_id __bspExtLock=0;

void
bspExtLock()
{
	assert ( RTEMS_SUCCESSFUL == rtems_semaphore_obtain(__bspExtLock, RTEMS_WAIT, RTEMS_NO_TIMEOUT ) );
}

void
bspExtUnlock()
{
	assert ( RTEMS_SUCCESSFUL == rtems_semaphore_release(__bspExtLock ) );
}

#ifdef __PPC__
extern void _bspExtMemProbeInit(void);
#endif

int bspExtVerbosity = 1;

rtems_status_code
bspExtInit(void)
{
rtems_status_code rval=RTEMS_SUCCESSFUL;
rtems_name n=rtems_build_name('B','E','L','K');

if (__bspExtLock) return rval; /* already initialized */

if (RTEMS_SUCCESSFUL!=(rval = rtems_semaphore_create(n,1,
	RTEMS_BINARY_SEMAPHORE | RTEMS_PRIORITY | RTEMS_INHERIT_PRIORITY,
	0,
	&__bspExtLock)))
	return rval;

#ifdef __PPC__
_bspExtMemProbeInit();
#endif

return rval;
}
