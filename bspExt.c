/* $Id$ */
#include "bspExt.h"

#include <stdio.h>
#include <bsp.h>

rtems_id __bspExtLock=0;

rtems_status_code
bspExtInit(void)
{
rtems_status_code rval=RTEMS_SUCCESSFUL;
rtems_name n=rtems_build_name('B','E','L','K');

if (RTEMS_SUCCESSFUL!=(rval = rtems_semaphore_create(n,1,
	RTEMS_SIMPLE_BINARY_SEMAPHORE,// | RTEMS_INHERIT_PRIORITY,
	0,
	&__bspExtLock)))
	return rval;

return rval;
}
