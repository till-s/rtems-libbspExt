#include "bspExt.h"
#include <malloc.h>
#include <assert.h>

long bspExtCacheLineSize=0;

/* determine the machine's cache line size */

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
