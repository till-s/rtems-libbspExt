
#include "bspExt.h"

#include <stdio.h>
#include <bsp.h>
#include <assert.h>

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

static void * memProbeByte(void *rval, void *from, void *to);
static void * memProbeShort(void *rval, void *from, void *to);
static void * memProbeLong(void *rval, void *from, void *to);
static void * memProbeEnd(void);


__asm__(
	".LOCAL memProbeByte, memProbeShort, memProbeLong, memProbeEnd\n"
	"memProbeByte:		\n"
	"	lbz %r4, 0(%r4) \n"
	"	stb %r4, 0(%r5) \n"
	"	blr		\n"
	"memProbeShort:		\n"
	"	lhz %r4, 0(%r4) \n"
	"	sth %r4, 0(%r5) \n"
	"	blr		\n"
	"memProbeLong:		\n"
	"	lwz %r4, 0(%r4) \n"
	"	stw %r4, 0(%r5) \n"
	"memProbeEnd:		\n"
	"	blr		\n"
);

extern int
_bspExtCatchDabr(BSP_Exception_frame*);

static void
bspExtExceptionHandler(BSP_Exception_frame* excPtr)
{
void *nip=(void*)excPtr->EXC_SRR0;
int	caughtDabr;

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
		return;
	} else if ((caughtDabr=_bspExtCatchDabr(excPtr))) {
		switch (caughtDabr) {
			default: /* should never get here */
			case -1: /* usr handler wants us to panic */
				break;

			case  1: /* phase 1; dabr deinstalled, complete data access */
			case  2: /* phase 2; stopped after completing faulting instruction */
				return;
		}
	}

	/* default handler */
	origHandler(excPtr);
}

void
_bspExtMemProbeInit(void)
{
rtems_interrupt_level	level;
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
rtems_status_code	rval=RTEMS_SUCCESSFUL;
unsigned char	buf[4];
MemProber	probe;
void		*faultAddr;

	/* lazy init */
	if (!origHandler) bspExtInit();

	assert(bspExtExceptionHandler==globalExceptHdl);

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
	if (write) {
		if (pval)
			memcpy(buf,pval,size);
		if ((faultAddr=probe(0,buf,addr)))
			rval=RTEMS_INVALID_ADDRESS;
	} else {
		if ((faultAddr=probe(0,addr,buf))) {
			rval=RTEMS_INVALID_ADDRESS;
			/* pass them the fault address back */
			if (pval)
				memcpy(pval,&faultAddr,size);
		} else {
			if (pval)
				memcpy(pval,buf,size);
		}
	}
	return rval;
}


#if 0
rtems_status_code
bspExtMemProbe(void *addr, int write, int size, void *pval)
{
rtems_status_code	rval=RTEMS_SUCCESSFUL;
void		  	*fac;
rtems_interrupt_level	level;
unsigned char		buf[4];

	memset(buf,'0',sizeof(buf));

	/* check arguments */
	if (!__bspExtLock) return RTEMS_NOT_DEFINED;
	if (!pval) pval=buf;

	/* obtain lock */
	if (RTEMS_SUCCESSFUL !=
		(rval=rtems_semaphore_obtain(__bspExtLock, RTEMS_WAIT, RTEMS_NO_TIMEOUT)))
		return rval;

	rtems_interrupt_disable(level);
	faultAddress = 0;
	/* install an exception handler to catch
	 * protection violations
	 */
	origHandler=globalExceptHdl;
	globalExceptHdl = catch_prot;

	/* make sure handler is swapped */
	__asm__ __volatile__ ("sync");

	/* note that taking the exception is context synchronizing */
	/* try to access memory */
	__asm__ __volatile__(
		"memProbeByteRead:	\n"
		"	lbz %0, 0(%1)	\n"
		"	b memProbeDone	\n"
		:
		:
		:);
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
	rtems_semaphore_release(__bspExtLock);
	return rval;
}
#endif
