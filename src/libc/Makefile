LIBC_OUTPUTNAME = libc
LIBK_OUTPUTNAME = libk

CC = gcc
ASMC = nasm

CFLAGS = -Wall -ffreestanding -fno-stack-protector -fno-exceptions -I../ -Os
ASMFLAGS =

SRCDIR = ./
LIBC_OBJDIR = ../../build/$(LIBC_OUTPUTNAME)
LIBK_OBJDIR = ../../build/$(LIBK_OUTPUTNAME)

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SRCDIR),*.c)
LIBC_OBJS = $(patsubst $(SRCDIR)/%.c, $(LIBC_OBJDIR)/%.o, $(SRC))
LIBK_OBJS = $(patsubst $(SRCDIR)/%.c, $(LIBK_OBJDIR)/%.o, $(SRC))

libc: $(LIBC_OBJS)

$(LIBC_OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@

libk: $(LIBK_OBJS)

$(LIBK_OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@ -DKERNEL_LIB

setup:
	@mkdir -p $(LIBC_OBJDIR)
	@mkdir -p $(LIBK_OBJDIR)

all:
	make setup
	make libc
	make libk