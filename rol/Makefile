#
# File:
#    Makefile
#
# Description:
#    Makefile for the coda primary and secondary readout lists 
#    running on an Intel-based controller running Linux
#
#
# $Date$
# $Rev$
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG=1
#

# Plug in your primary readout lists here..
VMEROL			= unblock_test.so unblock_test_ti.so unblock_test_fadc.so
# Add shared library dependencies here.  (jvme, ti, are already included)
#ROLLIBS			= 
ROLLIBS			= -lfadc -lremex -lsimple


LINUXVME_LIB	?= ${CODA}/extensions/linuxvme/libs
LINUXVME_INC	?= ${CODA}/extensions/linuxvme/include

# DEFs for compiling primary readout lists
CC			= gcc
AR                      = ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -g
else
CFLAGS			= -O3
endif
CFLAGS			+= -w -I. -I${LINUXVME_INC} -I/usr/include -I${CODA}/common/include\
			  -L. -L${LINUXVME_LIB}  -DJLAB
CFLAGS			+= -lrt -lpthread -ljvme -lti $(ROLLIBS)
CFLAGS			+= -DLINUX -DDAYTIME=\""`date`"\"

# DEFs for compiling CODA readout lists
CCRL			= ${CODA_BIN}/ccrl
CODA_INCS		= -I. -I${LINUXVME_INC} -I${CODA}/common/include
CODA_LIBDIRS            = -L. -L${LINUXVME_LIB}
CODA_LIBS		= -ljvme -lti $(ROLLIBS)
CODA_DEFS		= -DLINUX -DDAYTIME=\""`date`"\"
CODA_CFLAGS		= -O -w -fpic -shared ${CODA_INCS} ${CODA_LIBDIRS} \
			  ${CODA_LIBS} ${CODA_DEFS}
ifdef DEBUG
CODA_CFLAGS		+= -Wall -g
endif
CRLFILES		= $(wildcard *.crl)
CFILES			= $(CRLFILES:.crl=.c)
SOBJS			= $(CRLFILES:.crl=.so)


all: $(VMEROL) $(SOBJS) 

event_list_unblock_ti.c: event_list_unblock_ti.crl Makefile
	@echo
	@echo "Converting $< to $@"
	@${CCRL} $<

event_list_unblock_ti.so: event_list_unblock_ti.c
	@echo
	@echo "Building $@ from $<"
	@echo
	${CC} ${CODA_CFLAGS} -o $@ $<

event_list_unblock.c: event_list_unblock.crl Makefile
	@echo
	@echo "Converting $< to $@"
	@${CCRL} $<

event_list_unblock.so: event_list_unblock.c
	@echo
	@echo "Building $@ from $<"
	@echo
	${CC} ${CODA_CFLAGS} -o $@ $<

%.so: %.c Makefile tiprimary_list.c
	@echo
	@echo "Building $@ from $<"
	@echo
	$(CC) -fpic -shared  $(CFLAGS) -DINIT_NAME=$(@:.so=__init) -o $@ $<

clean distclean:
	@rm -f  $(VMEROL) $(SOBJS) $(CFILES) *~ 

.PHONY: all
