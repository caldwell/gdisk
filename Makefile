#  Copyright (c) 2008 David Caldwell,  All Rights Reserved.

CFLAGS += -MMD -std=gnu99 -Wall -Wno-parentheses
LDFLAGS += -lreadline

TARGETS = gdisk

all: $(TARGETS)

gdisk: gdisk.o guid.o mbr.o device.o autolist.o device-macosx.o

clean:
	rm -f *.o
	rm -f gdisk

-include *.d

# "make V=1" to see the make commands.
Q=$(if $V,,@)
ifeq ($V,)
_CC:=$(CC)
%.o: override CC=$(Q)printf "%15s  %s\n" "Compiling" $<; $(_CC)
_CXX:=$(CXX)
%.o: override CXX=$(Q)printf "%15s  %s\n" "Compiling" $<; $(_CXX)
_FC:=$(FC)
%.o: override FC=$(Q)printf "%15s  %s\n" "Compiling" $<; $(_FC)
_AS:=$(AS)
%.o: override AS=$(Q)printf "%15s  %s\n" "Assembling" $<; $(_AS)
_AR:=$(AR)
%.a: override AR=$(Q)printf "%15s  %s\n" "Archiving" $@; $(_AR)
$(TARGETS):override CC=$(Q)printf "%15s  %s\n" "Linking" $(@); $(_CC)
$(TARGETS):override CXX=$(Q)printf "%15s  %s\n" "Linking" $(@); $(_CXX)
endif
