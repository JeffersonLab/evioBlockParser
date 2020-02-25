#
# File:
#    Makefile
#
# Description:
#    Makefile for the SIMPLE library
#
# SVN: $Rev$
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)

BASENAME=simple
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

# Defs and build for Linux
CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L.
INCS			= -I.

LIBS			= lib${BASENAME}.a lib${BASENAME}.so


ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif

SRC			= ${BASENAME}Lib.c
HDRS			= $(SRC:.c=.h)
OBJ			= ${BASENAME}Lib.o
DEPS			= $(SRC:.c=.d)

all: ${LIBS}

%.o: %.c
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

ifeq ($(OS),LINUX)
links: $(LIBS)
	@echo " LN     $<"
	${Q}ln -sf $(PWD)/$< $(LINUXVME_LIB)/$<
	${Q}ln -sf $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
	${Q}ln -sf ${PWD}/*Lib.h $(LINUXVME_INC)

install: $(LIBS)
	@echo " CP     $<"
	${Q}cp $(PWD)/$< $(LINUXVME_LIB)/$<
	@echo " CP     $(<:%.a=%.so)"
	${Q}cp $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
	@echo " CP     ${BASENAME}Lib.h"
	${Q}cp ${PWD}/${BASENAME}Lib.h $(LINUXVME_INC)

%.d: %.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

endif

clean:
	@rm -vf ${BASENAME}Lib.{o,d} lib${BASENAME}.{a,so}

.PHONY: clean
