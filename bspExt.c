/* $Id$ */

/* initialization of libbspExt */

/*
 * Author: Till Straumann <strauman@@slac.stanford.edu>
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
#include <bsp.h>

rtems_id __bspExtLock=0;

extern void _bspExtMemProbeInit(void);

int bspExtVerbosity = 1;

rtems_status_code
bspExtInit(void)
{
rtems_status_code rval=RTEMS_SUCCESSFUL;
rtems_name n=rtems_build_name('B','E','L','K');

if (__bspExtLock) return rval; /* already initialized */

if (RTEMS_SUCCESSFUL!=(rval = rtems_semaphore_create(n,1,
	RTEMS_SIMPLE_BINARY_SEMAPHORE,// | RTEMS_INHERIT_PRIORITY,
	0,
	&__bspExtLock)))
	return rval;

_bspExtMemProbeInit();

return rval;
}
