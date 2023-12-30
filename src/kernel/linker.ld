OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
	. = 0xFFFFFFFF80000000;

	_kernelStart = .;

	.interrupts BLOCK(4K) : ALIGN(4K)
	{
		*(.interrupts)
	}

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	.smp_trampoline BLOCK(4K) : ALIGN(4K)
	{
		*(.smp_trampoline)
	}

	_kernelEnd = .;
}
