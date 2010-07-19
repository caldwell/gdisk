#  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

PLATFORM := $(shell uname -s | sed -e s/Linux/linux/ -e s/Darwin/macosx/)
DEBUG = -g
CFLAGS += -MMD -std=gnu99 -Wall -Wno-parentheses $(DEBUG) $(CFLAGS-$(PLATFORM))
LDFLAGS += $(DEBUG) $(LDFLAGS-$(PLATFORM))
LDLIBS += $(LDLIBS-$(PLATFORM))

TARGETS = gdisk

all: $(TARGETS)

gdisk: gdisk.o guid.o partition-type.o mbr.o device.o autolist.o csprintf.o human.o xmem.o device-$(PLATFORM).o

gdisk: LDLIBS += -lreadline -lz -lgc
gdisk: LDLIBS-linux += -luuid
gdisk.o: CFLAGS-macosx += -Drl_filename_completion_function=filename_completion_function

TAGS: *.c
	find . -name "*.[ch]" | etags -

clean:
	rm -f *.o
	rm -f $(TARGETS)

-include *.d

include Makefile.quiet
