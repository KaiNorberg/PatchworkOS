OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
	. = 0xFFFFFFFF80000000;

	_kernelStart = .;

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

	.worker_interrupts BLOCK(4K) : ALIGN(4K)
	{
		*(.interrupts)
	}

	.worker_trampoline BLOCK(4K) : ALIGN(4K)
	{
		*(.worker_trampoline)
	}

	_kernelEnd = .;
}
