# Makefile

build: beepcore swirl

include config


# ---
.PHONY: beepcore

LBEEP = beepcore-c/lib/libbeepcore.a
HBEEP = beepcore-c/include

beepcore: beepcore-c/lib beepcore-c/include $(LBEEP) $(HBEEP)/CBEEP.h

beepcore-c/lib beepcore-c/include:
	$(MKDIR) $@

CBEEP = $(wildcard beepcore-c/core/generic/*.c) $(wildcard beepcore-c/utility/*.c)
OBEEP = $(CBEEP:.c=.o)

$(LBEEP): $(OBEEP)
	$(AR) -r $@ $^

$(HBEEP)/CBEEP.h: beepcore-c/core/generic/CBEEP.h
	$(CP) $< $@


# ---
.PHONY: swirl

XLUA = $(wildcard lua/*.c)
HLUA = $(wildcard lua/*.h)
OLUA = $(XLUA:.c=.o)

LLUA=lua/$(SOSWIRL)


CFLAGS+=-Ibeepcore-c/include

swirl: $(LLUA)

test: $(LLUA)
	cd swirl && lua lua-test

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

