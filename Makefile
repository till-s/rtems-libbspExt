#
#  $Id$
#
# Templates/Makefile.leaf
# 	Template leaf node Makefile
#

APP=blah

RTMSLIBDIR=$(PROJECT_ROOT)/powerpc-rtems/mvme2307/lib


# C source names, if any, go here -- minus the .c
C_PIECES=memProbe
C_FILES=$(C_PIECES:%=%.c)
C_O_FILES=$(C_PIECES:%=${ARCH}/%.o)

# C++ source names, if any, go here -- minus the .cc
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

PGMS=${ARCH}/${APP}
PGM=${ARCH}/${APP}

# List of RTEMS managers to be included in the application goes here.
# Use:
#     MANAGERS=all
# to include all RTEMS managers in the application.
#MANAGERS=io event message rate_monotonic semaphore timer, etc.
#MANAGERS=io event semaphore timer
MANAGERS=all

include $(RTEMS_MAKEFILE_PATH)/Makefile.inc

include $(RTEMS_CUSTOM)
include $(RTEMS_ROOT)/make/leaf.cfg

#
# (OPTIONAL) Add local stuff here using +=
#

DEFINES  +=
CPPFLAGS +=
CFLAGS   += -O2

#
# CFLAGS_DEBUG_V are used when the `make debug' target is built.
# To link your application with the non-optimized RTEMS routines,
# uncomment the following line:
# CFLAGS_DEBUG_V += -qrtems_debug
#

#LD_PATHS  += xxx-your-EXTRA-library-paths-go-here, if any
#LD_LIBS   += xxx-your-libraries-go-here eg: -lvx
LDFLAGS   +=

#
# Add your list of files to delete here.  The config files
#  already know how to delete some stuff, so you may want
#  to just run 'make clean' first to see what gets missed.
#  'make clobber' already includes 'make clean'
#

#CLEAN_ADDITIONS += xxx-your-debris-goes-here
CLOBBER_ADDITIONS +=

all:	${ARCH} $(SRCS) $(PGMS) ${ARCH}/${APP}.bootimg

${ARCH}/${APP}: ${OBJS} ${LINK_FILES}
	$(CC) $(CFLAGS) -o $@ $(LINK_OBJS) $(LINK_LIBS)
#	$(make-exe)

${ARCH}/rtems.gz: ${ARCH}/${APP}
	$(RM) $@ ${ARCH}/rtems
	$(OBJCOPY) ${ARCH}/${APP} ${ARCH}/rtems -O binary -R .comment -S
	gzip -vf9 ${ARCH}/rtems

${ARCH}/${APP}.bootimg: ${ARCH}/${APP} ${ARCH}/rtems.gz
	(cd ${ARCH} ; $(LD) -o $(APP).bootimg $(RTMSLIBDIR)/bootloader.o --just-symbols=${APP} -bbinary rtems.gz -T$(RTMSLIBDIR)/ppcboot.lds -Map ${APP}.map)


# Install the program(s), appending _g or _p as appropriate.
# for include files, just use $(INSTALL_CHANGE)
install:  all
	$(INSTALL_VARIANT) -m 555 ${PGMS} ${PROJECT_RELEASE}/bin
