OUTPUTNAME = test

LDS = ../linker.ld
LD = ld
LDFLAGS = -T $(LDS) -Bsymbolic -nostdlib

CC = gcc
ASMC = nasm

CFLAGS = -Wall -ffreestanding -fno-stack-protector -fno-exceptions -I../ -Os
ASMFLAGS =

SRCDIR = ./
OBJDIR = ../../build/programs/$(OUTPUTNAME)
BINDIR = ../../bin/programs/

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SRCDIR),*.c)
ASMSRC = $(call rwildcard,$(SRCDIR),*.asm)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))
OBJS += $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%_ASM.o, $(ASMSRC))
DIRS = $(wildcard $(SRCDIR)/*)

build: $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJDIR)/%_ASM.o: $(SRCDIR)/%.asm
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@

link:
	$(LD) $(LDFLAGS) -o $(BINDIR)/$(OUTPUTNAME).elf $(OBJS)

setup:
	@mkdir -p $(SRCDIR)
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

all:
	make setup
	make build
	make link