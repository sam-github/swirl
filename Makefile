# Makefile

build: beepcore swirl

include config


# ---
.PHONY: beepcore

LBEEP = beepcore-c/lib/libbeepcore.a
IBEEP = beepcore-c/include

beepcore: beepcore-c/lib beepcore-c/include $(LBEEP) $(IBEEP)/CBEEP.h

beepcore-c/lib beepcore-c/include:
	$(MKDIR) $@

CBEEP = $(wildcard beepcore-c/core/generic/*.c) $(wildcard beepcore-c/utility/*.c)
HBEEP = $(wildcard beepcore-c/core/generic/*.h) $(wildcard beepcore-c/utility/*.h)
OBEEP = $(CBEEP:.c=.o)

$(OBEEP): $(HBEEP)

$(LBEEP): $(OBEEP)
	$(AR) -r $@ $^

$(IBEEP)/CBEEP.h: beepcore-c/core/generic/CBEEP.h
	$(CP) $< $@


# ---
.PHONY: swirl

XLUA = $(wildcard lua/*.c)
HLUA = $(wildcard lua/*.h)
OLUA = $(XLUA:.c=.o)

LLUA=lua/$(SOSWIRL)


CFLAGS+=-Ibeepcore-c/include

swirl: $(LLUA) lua/API.txt

lua/API.txt: lua/swirl.lua lua/swirl.c lua/swirlsock.lua lua/sockext.lua
	-ldoc $^ > $@_ && mv $@_ $@

test: $(LLUA)
	cd lua && lua gc-test
	cd lua && lua core-test
	cd lua && lua quote-test

$(OLUA): CFLAGS+=$(LUACFLAGS)
$(OLUA): $(HLUA)

$(LLUA): LDFLAGS+=$(LUALDFLAGS)
$(LLUA): $(OLUA) $(LBEEP)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# ---
.PHONY: install

LSHARE=lua/swirl.lua lua/sockext.lua lua/swirlsock.lua

install:
	$(MKDIR) $(INSTALL_TOP_LIB)
	$(MKDIR) $(INSTALL_TOP_SHARE)
	$(CP) $(LLUA) $(INSTALL_TOP_LIB)
	$(CP) $(LSHARE) $(INSTALL_TOP_SHARE)

