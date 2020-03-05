#
# File:
#    Makefile
#
# Description:
#      Build recipes for testing evioBlockParser
#
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

EVIO_INC	?= /site/coda/3.10/Linux-x86_64/include
EVIO_LIB	?= /site/coda/3.10/Linux-x86_64/lib


CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)g++
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I.. -I${EVIO_INC}
CFLAGS			= -Wall -g -std=c++11 \
			  -L. -L.. -L${EVIO_LIB} \
			  -levioBlockParser -levioxx -levio -lexpat

SRC			= simpleScan.cc
PROGS			= $(SRC:.cc=)
DEPS			= $(SRC:.cc=.d)

all: $(PROGS)

clean:
	@rm -f $(PROGS) $(DEPS) *~

%: %.cc
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -o $@ $<

%.d: %.cc
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: clean
