#  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

PLATFORM := $(shell uname -s | sed -e s/Linux/linux/ -e s/Darwin/macosx/)
DEBUG = -g
CFLAGS += -MMD -std=gnu99 -Wall -Wno-parentheses $(DEBUG) $(CFLAGS-$(PLATFORM))
LDFLAGS += $(DEBUG) $(LDFLAGS-$(PLATFORM))
LDLIBS += $(LDLIBS-$(PLATFORM))

TARGETS = gdisk

all: $(TARGETS)

gdisk: gdisk.o guid.o partition-type.o mbr.o device.o autolist.o csprintf.o human.o xmem.o dalloc.o device-$(PLATFORM).o

gdisk: LDLIBS += -lreadline -lz
gdisk: LDLIBS-linux += -luuid
gdisk.o gdisk.E: CFLAGS-macosx += -Drl_filename_completion_function=filename_completion_function

%.E: %.c Makefile
	$(CC) -E -o $@ $(CFLAGS) $(CPPFLAGS) $<

TAGS: *.c
	find . -name "*.[ch]" | etags -

clean:
	rm -f *.o
	rm -f $(TARGETS)

-include *.d

include Makefile.quiet
