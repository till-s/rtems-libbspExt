#ifndef TILL_BSP_EXTENSION_H
#define TILL_BSP_EXTENSION_H

#include <rtems.h>

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
 */

rtems_status_code
bspExtMemProbe(void *addr, int write, int size, void *pval);

/* cache line size of this machine */
extern long bspExtCacheLineSize;

#define	CACHE_UTIL_DECL(name, asminst)\
inline void \
name(char *start, char *end)\
{\
register int inc=bspExtCacheLineSize;\
	while (start < end) {\
		__asm__ __volatile__ (asminst::"r"(start));\
		start+=inc;\
	}\
}

CACHE_UTIL_DECL(bspExtFlushCacheLines, "dcbf 0, %0");
CACHE_UTIL_DECL(bspExtStoreCacheLines, "dcbst 0, %0");
CACHE_UTIL_DECL(bspExtInvalCacheLines, "dcbi 0, %0");
CACHE_UTIL_DECL(bspExtClearCacheLines, "dcbz 0, %0");

#endif
