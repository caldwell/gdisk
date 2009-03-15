#  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

PLATFORM := $(shell uname -s | sed -e s/Linux/linux/ -e s/Darwin/macosx/)
DEBUG = -g
CFLAGS += -MMD -std=gnu99 -Wall -Wno-parentheses $(DEBUG)
LDFLAGS += $(DEBUG)

TARGETS = gdisk

all: $(TARGETS)

gdisk: gdisk.o guid.o partition-type.o mbr.o device.o autolist.o csprintf.o human.o device-$(PLATFORM).o

gdisk: LDLIBS += -lreadline -lz

ifeq ($(PLATFORM), linux)
  gdisk: LDLIBS += -luuid
endif

ifeq ($(PLATFORM),macosx)
  gdisk.o: CFLAGS += -Drl_filename_completion_function=filename_completion_function
endif

clean:
	rm -f *.o
	rm -f $(TARGETS)

-include *.d

include Makefile.quiet
