#
#  $Id$
#
# Templates/Makefile.lib
#       Template library Makefile
#


LIBNAME=libbspExt.a        # xxx- your library names goes here
LIB=${ARCH}/${LIBNAME}

# C and C++ source names, if any, go here -- minus the .c or .cc
C_PIECES=bspExt isrWrap
C_PIECES_powerpc=memProbe dabrBpnt
C_PIECES+=$(C_PIECES_$(RTEMS_CPU))
C_FILES=$(C_PIECES:%=%.c)
C_O_FILES=$(C_PIECES:%=${ARCH}/%.o)

CC_PIECES=
CC_FILES=$(CC_PIECES:%=%.cc)
CC_O_FILES=$(CC_PIECES:%=${ARCH}/%.o)

H_FILES=bspExt.h

# Assembly source names, if any, go here -- minus the .S
S_PIECES=
S_FILES=$(S_PIECES:%=%.S)
S_O_FILES=$(S_FILES:%.S=${ARCH}/%.o)

SRCS=$(C_FILES) $(CC_FILES) $(H_FILES) $(S_FILES)
OBJS=$(C_O_FILES) $(CC_O_FILES) $(S_O_FILES)

include $(RTEMS_MAKEFILE_PATH)/Makefile.inc

include $(RTEMS_CUSTOM)
include $(RTEMS_ROOT)/make/lib.cfg

#
# Add local stuff here using +=
#

DEFINES  +=
CPPFLAGS +=
# inline declarations require -O
CFLAGS   += -Winline

#
# Add your list of files to delete here.  The config files
#  already know how to delete some stuff, so you may want
#  to just run 'make clean' first to see what gets missed.
#  'make clobber' already includes 'make clean'
#

CLEAN_ADDITIONS +=
CLOBBER_ADDITIONS +=

all:	${ARCH} $(SRCS) $(LIB)

$(LIB): ${OBJS}
	$(make-library)

ifndef RTEMS_SITE_INSTALLDIR
RTEMS_SITE_INSTALLDIR = $(PROJECT_RELEASE)
INSTINCDIR=${RTEMS_SITE_INSTALLDIR}/lib/include/
else
INSTINCDIR=${RTEMS_SITE_INSTALLDIR}/include/
endif
INSTLIBDIR=${RTEMS_SITE_INSTALLDIR}/lib/

${INSTLIBDIR} \
${INSTINCDIR}/bsp:
	test -d $@ || mkdir -p $@

# Install the library, appending _g or _p as appropriate.
# for include files, just use $(INSTALL_CHANGE)
install:  all ${INSTLIBDIR} ${INSTINCDIR}/bsp
	$(INSTALL_VARIANT) -m 644 ${LIB} ${INSTLIBDIR}
	$(INSTALL_CHANGE) -m 644 ${H_FILES} ${INSTINCDIR}/bsp/

REVISION=$(filter-out $$%,$$Name$$)
tar:
	@$(make-tar)
