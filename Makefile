#
# File:
#    Makefile
#
# Description:
#    Makefile for the evioBlockParser library
#
# SVN: $Rev$
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
BASENAME = evioBlockParser
#
#
EVIO_LIB	?=	/site/coda/3.10/Linux-x86_64/lib
EVIO_INC	?=	/site/coda/3.10/Linux-x86_64/include

#

# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG	?= 1
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

CC			= g++
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -std=c++11 -L. -L${EVIO_LIB}
INCS			= -I. -I${EVIO_INC}

LIBS			= lib${BASENAME}.so lib${BASENAME}.a


ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif

SRC			= ${BASENAME}.cc
HDRS			= $(SRC:.cc=.hxx)
OBJ			= ${BASENAME}.o
DEPS			= $(SRC:.cc=.d)

all: ${LIBS}

%.o: %.cc
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -c -o $@ $(SRC)

%.so: $(SRC)
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(SRC)

%.a: $(OBJ)
	@echo " AR     $@"
	${Q}$(AR) ru $@ $<
	@echo " RANLIB $@"
	${Q}$(RANLIB) $@

%.d: %.cc
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

clean:
	@rm -vf ${OBJ} ${DEPS} ${LIBS}

.PHONY: clean
