TASKBAR_OUT = bin/programs/taskbar.elf

TASKBAR_SRC = $(wildcard src/programs/taskbar/*.c)

TASKBAR_OBJ = $(patsubst src/%, build/taskbar/%.o, $(TASKBAR_SRC))

build/taskbar/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -c -o $@ $<

$(TASKBAR_OUT): $(TASKBAR_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(TASKBAR_OUT)
