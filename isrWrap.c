/* $Id$ */

/* Fix the broken IRQ API of RTEMS;
 * 
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2003
 *
 * On some BSPs, RTEMS does not allow for passing an argument to
 * ISRs and ISRs can not be shared. This is especially annoying
 * on PCI where shared IRQs should be available.
 * 
 * Not having an ISR argument leads to subtle problems when
 * shared IRQs are involved: consider the case of a driver
 * supporting multiple, identical devices listening on different
 * interrupt lines with different priorities. The driver's
 * global ISR cannot just poll all devices for activity:
 *
 *   ISR() {
 *     for ( i=0; i<NDEVS; i++) {
 *         if (irq_pending(dev[i])) {
 *            handle_irq(dev[i]);
 *         }
 *     }
 *   }
 *
 * is NOT CORRECT and (unless the device provides atomical
 * 'test and clear' of the IRQ condition) contains a race
 * condition:
 *
 * Let's say device # 1 raises an IRQ on line 1
 * and just between the ISR() executing 'irq_pending()' and 'handle_irq()',
 * clearing the IRQ, device #2 raises another line with higher
 * priority. The preempting ISR then also finds #1's IRQ pending
 * and executes the 'handle_irq()' sequence!
 */
#include <rtems.h>
#include <bsp.h>
#include <bsp/irq.h>
#include <assert.h>
#include <stdlib.h>

#include "bspExt.h"

typedef volatile struct ISRRec_ {
	void  *uarg;
	void (*hdl)(void*);
	volatile struct ISRRec_ *next;
	int   flags;
} ISRRec, * volatile ISR;

typedef struct WrapRec_ {
	void	(* const wrapper)(void);
	volatile int	irqLine;
	ISR	anchor;
} WrapRec, *Wrap;

#define DECL_WRAPPER(num) static void wrap##num() { isrdispatch(wrappers+num); }
#define SLOTDECL(num) { wrap##num, 0, 0 }

#define NumberOf(arr) (sizeof(arr)/sizeof((arr)[0]))

static void isrdispatch(Wrap w) { register ISR i; for (i=w->anchor; i; i=i->next) i->hdl(i->uarg); }

static WrapRec wrappers[];

/* there should at least be enough wrappers for the 4 PCI interrupts; provide some more... */
DECL_WRAPPER(0)
DECL_WRAPPER(1)
DECL_WRAPPER(2)
DECL_WRAPPER(3)
DECL_WRAPPER(4)
DECL_WRAPPER(5)
DECL_WRAPPER(6)

static WrapRec wrappers[] = {
	SLOTDECL(0),
	SLOTDECL(1),
	SLOTDECL(2),
	SLOTDECL(3),
	SLOTDECL(4),
	SLOTDECL(5),
	SLOTDECL(6),
};

static void noop(const rtems_irq_connect_data *unused) {};
static int  noop1(const rtems_irq_connect_data *unused) { return 0;};

int
bspExtRemoveSharedISR(int irqLine, void (*isr)(void *), void *uarg)
{
Wrap w;
ISR	*preq;

	assert( irqLine >= 0 );

	bspExtLock();

		for ( w = wrappers; w < wrappers + NumberOf(wrappers); w++ )
			if ( w->irqLine == irqLine ) {
				for ( preq = &w->anchor; *preq; preq = &(*preq)->next ) {
					if ( (*preq)->hdl == isr && (*preq)->uarg == uarg ) {
							ISR found = *preq;
							/* atomic; there's no need to disable interrupts */
							*preq = (*preq)->next;
							if ( 0 == w->anchor) {
								/* was the last one; release the wrapper slot */
								rtems_irq_connect_data d;
								d.on   = d.off = noop;
								d.isOn = noop1;

								d.hdl  = w->wrapper;
								d.name = w->irqLine;
								assert ( BSP_remove_rtems_irq_handler(&d) );
								w->irqLine = -1;
							}
							bspExtUnlock();
							free((void*)found);
							return 0;
					}
				}
			}
	bspExtUnlock();
	return -1;
}

int
bspExtInstallSharedISR(int irqLine, void (*isr)(void *), void * uarg, int flags)
{
Wrap w;
Wrap avail=0;

ISR	req = malloc(sizeof(*req));

	assert( req );
	req->hdl   = isr;
	req->uarg  = uarg;
	req->flags = flags;

	assert( irqLine >= 0 );

	bspExtLock();
		for ( w=wrappers; w<wrappers+NumberOf(wrappers); w++ ) {
			if ( w->anchor ) {
				if ( w->irqLine == irqLine ) {
					if ( BSPEXT_ISR_NONSHARED & (flags | w->anchor->flags) ) {
						/* we either requested exclusive access or
						 * the already installed ISR has this flag set
						 */
						goto bailout;
					}
					req->next = w->anchor;
					/* atomic; there's no need to disable interrupts - even if an interrupt happens right now */
					w->anchor = req;
					req = 0;
					break;
				}
			} else {
				/* found a free slot, remember */
				avail = w;
			}
		}
		if ( req && avail ) {
			/* no wrapper installed yet for this line but a slot is available */
			rtems_irq_connect_data d;
			d.on   = d.off = noop;
			d.isOn = noop1;

			d.hdl  = avail->wrapper;
			d.name = irqLine;
			avail->irqLine = irqLine;

			if ( BSP_install_rtems_irq_handler(&d) ) {
				/* this should be safe; if an IRQ happens isrdispatch() simply will find anchor == 0 */
				req->next = avail->anchor;
				avail->anchor = req;
				req = 0;
			}
		}
bailout:
	bspExtUnlock();
	free((void*)req);
	return req != 0;
}
