TOPSRC		= $(shell cd ../..; pwd)
include $(TOPSRC)/target.mk

CFLAGS		+= -Werror -Wall

SRCS		= retro.c
OBJS		= retro.o
BIN             = retroforth
LIBS            += -ltermcap

all:            install-lib

$(BIN).elf:     ${OBJS}
		${CC} ${LDFLAGS} -o $(BIN).elf $(OBJS) ${LIBS}

$(BIN).dis:     $(BIN).elf
		${OBJDUMP} -S $(BIN).elf > $(BIN).dis
		${SIZE} $(BIN).elf

../$(BIN):      $(BIN).dis
		${ELF2AOUT} $(BIN).elf $@

install-lib:    ../$(BIN)
		cp retroImage $(TOPSRC)/lib

clean:
		rm -f *.o *.elf ../$(BIN) *.elf *.dis *~ retro.c $(TOPSRC)/lib/retroImage
