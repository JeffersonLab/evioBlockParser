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
DEBUG=1
#
LINUXVME_LIB		?= /site/coda/3.10/linuxvme/lib
LINUXVME_INC		?= /site/coda/3.10/linuxvme/include

CC			= gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -I. -I${LINUXVME_INC} \
			  -L. -L${LINUXVME_LIB}

LIBS			= libsimple.a

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif

SRC			= simpleLib.c
HDRS			= $(SRC:.c=.h)
OBJ			= simpleLib.o

all: echoarch $(LIBS)

$(OBJ): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $(SRC)

$(LIBS): $(OBJ)
	$(CC) -fpic -shared $(CFLAGS) -o $(@:%.a=%.so) $(SRC)
	$(AR) ruv $@ $<
	$(RANLIB) $@

clean:
	@rm -vf simpleLib.o libsimple.{a,so}

echoarch:
	@echo "Make for $(ARCH)"

.PHONY: clean echoarch
