OUTPUTNAME = libc

CC = gcc
ASMC = nasm

CFLAGS = -Wall -ffreestanding -fno-stack-protector -fno-exceptions -I../ -Os
ASMFLAGS =

SRCDIR = ./
OBJDIR = ../../build/$(OUTPUTNAME)
BINDIR = ../../bin/$(OUTPUTNAME)

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SRCDIR),*.c)
ASMSRC = $(call rwildcard,$(SRCDIR),*.asm)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))
OBJS += $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%_ASM.o, $(ASMSRC))
DIRS = $(wildcard $(SRCDIR)/*)

libc: $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJDIR)/%_ASM.o: $(SRCDIR)/%.asm
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@

setup:
	@mkdir -p $(BINDIR)
	@mkdir -p $(SRCDIR)
	@mkdir -p $(OBJDIR)

all:
	make setup
	make libc