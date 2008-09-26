CFLAGS=-g -Wall -Werror
LDFLAGS=-g

C=$(wildcard *.c ../../utility/*.c)
O=$(C:.c=.o)

libbeepcore.a: $(O)
	$(AR) -r $@ $^


