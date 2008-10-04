#  $Id$
AUTOMAKE_OPTIONS=foreign

include $(top_srcdir)/rtems-pre.am

C_PIECES_powerpc=memProbe.c dabrBpnt.c

libbspExt_a_SOURCES=bspExt.c isrWrap.c
EXTRA_libbspExt_a_SOURCES = $(C_PIECES_powerpc)
libbspExt_a_LIBADD       =$(C_PIECES_@rtems_cpu@:%.c=%.$(OBJEXT))
libbspExt_a_DEPENDENCIES =$(C_PIECES_@rtems_cpu@:%.c=%.$(OBJEXT))

lib_LIBRARIES=libbspExt.a

include_bsp_HEADERS=bspExt.h

include $(top_srcdir)/rtems.am
