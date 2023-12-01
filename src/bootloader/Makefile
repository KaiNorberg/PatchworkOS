OUTPUTNAME = bootx64

CC = gcc
LD = ld

CFLAGS = -Os -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -I$(GNU_EFI)/inc -Wall -Werror

SRCDIR = ./
BINDIR = ../../bin/$(OUTPUTNAME)
OBJDIR = ../../build/$(OUTPUTNAME)
GNU_EFI = ../../vendor/gnu-efi

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SRCDIR),*.c)
ASMSRC = $(call rwildcard,$(SRCDIR),*.asm)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC))
DIRS = $(wildcard $(SRCDIR)/*)

setup:
	@mkdir -p $(SRCDIR)
	@mkdir -p $(BINDIR)
	@mkdir -p $(OBJDIR)	

bootloader: $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo !==== COMPILING $^
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@

link: 
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(OBJS) -o $(BINDIR)/$(OUTPUTNAME).so -lgnuefi -lefi

	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BINDIR)/$(OUTPUTNAME).so $(BINDIR)/$(OUTPUTNAME).efi

all:
	make bootloader
	make link