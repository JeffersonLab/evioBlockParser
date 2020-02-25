#
# File:
#    Makefile
#
# Description:
#      Build recipes for testing the simple library
#

EVIO_INC	?= /site/coda/3.10/Linux-x86_64/include
EVIO_LIB	?= /site/coda/3.10/Linux-x86_64/lib


CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -Wall -g -I. -I.. -I${EVIO_INC} \
			  -L. -L.. -L${EVIO_LIB} \
			  -lsimple -levio

PROGS			= simpleScan

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~

%: %.c
	echo "Making $@"
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c)  -lrt

.PHONY: all clean distclean
