# Makefile

build: beepcore swirl

include config

.PHONY: beepcore

LBEEP = beepcore-c/lib/libbeepcore.a
HBEEP = beepcore-c/include

beepcore: beepcore-c/lib beepcore-c/include $(LBEEP) $(HBEEP)/CBEEP.h

beepcore-c/lib beepcore-c/include:
	mkdir -p $@

CBEEP = $(wildcard beepcore-c/core/generic/*.c) $(wildcard beepcore-c/utility/*.c)
OBEEP = $(CBEEP:.c=.o)

$(LBEEP): $(OBEEP)
	$(AR) -r $@ $^

$(HBEEP)/CBEEP.h: beepcore-c/core/generic/CBEEP.h
	cp -v $< $@


.PHONY: swirl

XLUA = $(wildcard swirl/*.c)
HLUA = $(wildcard swirl/*.h)
OLUA = $(XLUA:.c=.o)

LLUA=swirl/swirl.so


CFLAGS+=-Ibeepcore-c/include

swirl: $(LLUA)

test: $(LLUA)
	cd swirl && lua test.lua

$(OLUA): CFLAGS+=$(LUACFLAGS)
$(OLUA): $(HLUA)

$(LLUA): LDFLAGS+=$(LUALDFLAGS)
$(LLUA): $(OLUA) $(LBEEP)
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

