/* $Id$ */

/* determine the machine's cache line size */

/* Author: Till Straumann <strauman@slac.stanford.edu>, 2001 */

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
#include <malloc.h>
#include <assert.h>

long bspExtCacheLineSize=0;

/* determine the machine's cache line size
 * NOTE: this works only on an enabled cache
 *       it raises an exception otherwise...
 */

#define MAX_DETECT 128
long __bspExtGetCacheLineSize(void)
{
	unsigned char *tmp, *p, *q;
	/* get a buffer */
	tmp=malloc(3*MAX_DETECT);
	/* fill with nonzero values */
	memset(tmp, 0xff, 3*MAX_DETECT);
	/* align to MAX_DETECT */
	p = (unsigned char*)(((unsigned long)tmp + MAX_DETECT - 1) & ~(MAX_DETECT - 1));
	/* clear one cache line */
	__asm__ __volatile__("dcbz 0, %0"::"r"(p));
	/* search for the last 0 */
	for (q=p; q<tmp+3*MAX_DETECT && !*q; q++);
	free(tmp);
	/* make sure it's less than MAX_DETECT */
	assert(q-p <= MAX_DETECT);
	return q-p;
}
