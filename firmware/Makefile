H		= ./uip
M		= .
S		= $(H)

vpath %.c $(M):$(S)

ifndef GCCPREFIX
    GCCPREFIX   = /usr/local/pic32-tools/bin/pic32-
endif


DEFS		+= -I. -I$(H)

LDRFILE		= ./bare-metal.ld 
#LDRFILE		= ./loader/bare-metal.ld 
#LDRFILE		= bootldr.ld 

DEPFLAGS	= -MT $@ -MD -MP -MF .deps/$*.dep
CFLAGS		= -O $(DEFS) $(DEPFLAGS)
ASFLAGS		= $(DEFS) $(DEPFLAGS)

DEVICE	= 32MX764F128H
AVRDUDE = /usr/local/pic32-tools/tools/avrdude
AVRDUDECONF = -C /usr/local/pic32-tools/tools/avrdude.conf -c stk500v2 \
		-p pic32 -P /dev/ttyUSB0 -q -q

PIC32PROG = /usr/local/bin/pic32prog
PIC32PROGCONF = -p

CC		= $(GCCPREFIX)gcc -EL -g -mips32r2 -mprocessor=$(DEVICE) -O3
CC		+= -Wextra -Wall -fno-exceptions -ffunction-sections
CC		+= -fdata-sections -mno-smart-io 
LDFLAGS         += -T $(LDRFILE) -lc -lm -Wl,-Map=rt-8p8c.map,--gc-sections
LDFLAGS         += --defsym=_min_heap_size=8000 --defsym=_min_stack_size=1048
SIZE		= $(GCCPREFIX)size
OBJDUMP		= $(GCCPREFIX)objdump
OBJCOPY		= $(GCCPREFIX)objcopy
BIN2HEX		= $(GCCPREFIX)bin2hex

SRCOBJ	= main.o timer.o uip.o uip_arp.o etherdev.o stepgen.o

.SUFFIXES:

.SUFFIXES:	.hex .dis .c .elf .o

all:		.deps rt-8p8c.elf rt-8p8c.hex load
		$(SIZE) rt-8p8c.elf
		
clean:
		rm -rf .deps *.o *.elf *.bin *.dis *.map *.hex *.dep

.deps:
		mkdir .deps


rt-8p8c.elf:	$(SRCOBJ) $(LDRFILE)
		$(CC) $(LDFLAGS) $(SRCOBJ) -o $@
		chmod -x $@
		$(OBJDUMP) -d -S $@ > rt-8p8c.dis
		$(BIN2HEX) $@

load:           rt-8p8c.elf
		$(PIC32PROG) $(PIC32PROGCONF) rt-8p8c.hex
.o.dis:
		$(OBJDUMP) -d -z -S $< > $@

ifeq (.deps, $(wildcard .deps))
-include .deps/*.dep
endif
