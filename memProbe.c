
#include "bspExt.h"

#include <stdio.h>
#include <bsp.h>

static exception_handler_t	save=0;
static void *faultAddress=0;

static void
catch_prot(BSP_Exception_frame* excPtr)
{
	if (faultAddress) {
		/* nested call */
		save(excPtr);
	} else {
		faultAddress=excPtr->EXC_DAR;
		excPtr->EXC_SRR0+=4; /* skip faulting instruction */
	}
/*__asm__ __volatile__("mfspr %0, %1": "=r" (faultAddress) : "i"(DAR));*/
}

static rtems_id bspExtLock=0;

rtems_status_code
bspExtInit(void)
{
rtems_name n=rtems_build_name('B','E','L','K');
return rtems_semaphore_create(n,1,
	RTEMS_SIMPLE_BINARY_SEMAPHORE,// | RTEMS_INHERIT_PRIORITY,
	0,
	&bspExtLock);
}

rtems_status_code
bspExtMemProbe(void *addr, int write, int size, void *pval)
{
rtems_status_code	rval=RTEMS_SUCCESSFUL;
void		  	*fac;
rtems_interrupt_level	level;
unsigned char		buf[4];

	memset(buf,'0',sizeof(buf));

	/* check arguments */
	if (!bspExtLock) return RTEMS_NOT_DEFINED;
	if (!pval) pval=buf;

	/* obtain lock */
	if (RTEMS_SUCCESSFUL !=
		(rval=rtems_semaphore_obtain(bspExtLock, RTEMS_WAIT, RTEMS_NO_TIMEOUT)))
		return rval;

	rtems_interrupt_disable(level);
	faultAddress = 0;
	/* install an exception handler to catch
	 * protection violations
	 */
	save=globalExceptHdl;
	globalExceptHdl = catch_prot;
	/* note that taking the exception is context synchronizing */
	/* try to access memory */
#define CASE(type)	\
	case sizeof(type): if (write) *(type *) addr = *(type *)pval; \
                           else       *(type *) pval = *(type *)addr; \
			   break
	switch (size) {
		CASE(unsigned char);
		CASE(unsigned short);
		CASE(unsigned long);
		default: /* should never get here */
			globalExceptHdl = save;
			rtems_interrupt_enable(level);
			rval=RTEMS_INVALID_SIZE;
			goto cleanup;
	}
	/* get a copy of the fault address before re-enabling
	 * interrupts (could be that somebody else generates
	 * an access violation exception here)
	 */
	__asm__ __volatile__ ("sync");
	fac = faultAddress;
	globalExceptHdl = save;
	__asm__ __volatile__ ("sync");
	rtems_interrupt_enable(level);

	if (fac) {
		rval = RTEMS_INVALID_ADDRESS;
	}

	/* on error return RTEMS_INVALID_ADDRESS */
cleanup:
	rtems_semaphore_release(bspExtLock);
	return rval;
}
