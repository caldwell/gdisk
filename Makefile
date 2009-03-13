#  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

PLATFORM := $(shell uname -s | sed -e s/Linux/linux/ -e s/Darwin/macosx/)
CFLAGS += -MMD -std=gnu99 -Wall -Wno-parentheses
LDFLAGS += -lreadline

TARGETS = gdisk

all: $(TARGETS)

gdisk: gdisk.o guid.o mbr.o device.o autolist.o csprintf.o device-$(PLATFORM).o

clean:
	rm -f *.o
	rm -f $(TARGETS)

-include *.d

include Makefile.quiet
