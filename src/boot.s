# https://wiki.osdev.org/Higher_Half_x86_Bare_Bones 

/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/* 
Declare a multiboot header that marks the program as a kernel. These are magic
values that are documented in the multiboot standard. The bootloader will
search for this signature in the first 8 KiB of the kernel file, aligned at a
32-bit boundary. The signature is in its own section so the header can be
forced to be within the first 8 KiB of the kernel file.
*/
# .section .multiboot
.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/*
The multiboot standard does not define the value of the stack pointer register
(esp) and it is up to the kernel to provide a stack. This allocates room for a
small stack by creating a symbol at the bottom of it, then allocating 16384
bytes for it, and finally creating a symbol at the top. The stack grows
downwards on x86. The stack is in its own section so it can be marked nobits,
which means the kernel file is smaller because it does not contain an
uninitialized stack. The stack on x86 must be 16-byte aligned according to the
System V ABI standard and de-facto extensions. The compiler will assume the
stack is properly aligned and failure to align the stack will result in
undefined behavior.
*/
.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# Preallocate pages used for paging. Don't hard-code addresses and assume they
# are available, as the bootloader might have loaded its multiboot structures or
# modules there. This lets the bootloader know it must avoid the addresses.
.section .boot_pages, "aw", @nobits
	.align 4096
boot_page_directory:
	.skip 4096
boot_page_table1:
	.skip 4096
# Further page tables may be required if the kernel grows beyond 3 MiB.


/*
The linker script specifies _start as the entry point to the kernel and the
bootloader will jump to this position once the kernel has been loaded. It
doesn't make sense to return from this function as the bootloader is gone.
*/
# .section .text
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
	movl $(boot_page_table1 - 0xC0000000), %edi
	movl $0, %esi

1:
	cmpl $_kernel_start, %esi
	jl 2f
	cmpl $(_kernel_end - 0xC0000000), %esi
	jge 3f

	# Map physical address as "present, writable". Note that this maps
	# .text and .rodata as writable. Mind security and map them as non-writable.
	movl %esi, %edx
	orl $0x003, %edx
	movl %edx, (%edi)

2:
	# Size of page is 4096 bytes.
	addl $4096, %esi
	# Size of entries in boot_page_table1 is 4 bytes.
	addl $4, %edi
	# Loop to the next entry if we haven't finished.
	loop 1b

3:
	# Map VGA video memory to 0xC03FF000 as "present, writable".
	movl $(0x000B8000 | 0x003), boot_page_table1 - 0xC0000000 + 1023 * 4

	# Map the page table to both virtual addresses 0x00000000 and 0xC0000000.
	movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 0
	movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 768 * 4

	# Map the last entry to the directory
	movl $(boot_page_directory - 0xC0000000 + 0x3), boot_page_directory - 0xC0000000 + 1023 * 4

	# Set cr3 to the address of the boot_page_directory.
	movl $(boot_page_directory - 0xC0000000), %ecx
	# movl $(boot_page_directory), %ecx
	movl %ecx, %cr3

	# Enable paging and the write-protect bit.
	movl %cr0, %ecx
	orl $0x80010000, %ecx
	movl %ecx, %cr0

	# Jump to higher half with an absolute jump. 
	lea 4f, %ecx
	jmp *%ecx

.section .text
4:
	# Unmap the identity mapping as it is now unnecessary. 
	movl $0, boot_page_directory + 0 
	
	# Reload crc3 to force a TLB flush so the changes to take effect.
	movl %cr3, %ecx
	movl %ecx, %cr3
	
	mov $stack_top, %esp
	
	call main

	cli
1:	hlt
	jmp 1b

/*
Set the size of the _start symbol to the current location '.' minus its start.
This is useful when debugging or when you implement call tracing.
*/
# .size _start, . - _start

.global gdt_flush
.type gdt_flush, @function
gdt_flush:
	# Load the GDT
    lgdt gp
    # Flush the values to 0x10
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
	ljmp $0x08, $flush2
flush2:
    ret

.global idt_load
.type idt_load, @function
idt_load:
	lidt idtp
	ret

# In just a few pages in this tutorial, we will add our Interrupt
# Service Routines (ISRs) right here!
.global isr0
.global isr1
.global isr2
.global isr3
.global isr4
.global isr5
.global isr6
.global isr7
.global isr8
.global isr9
.global isr10
.global isr11
.global isr12
.global isr13
.global isr14
.global isr15
.global isr16
.global isr17
.global isr18
.global isr19
.global isr20
.global isr21
.global isr22
.global isr23
.global isr24
.global isr25
.global isr26
.global isr27
.global isr28
.global isr29
.global isr30
.global isr31

#  0: Divide By Zero Exception
isr0:
	cli
	pushl $0
	pushl $0
	jmp isr_common_stub

#  1: Debug Exception
isr1:
	cli
	pushl $0
	pushl $1
	jmp isr_common_stub

#  2: Non Maskable Interrupt Exception
isr2:
	cli
	pushl $0
	pushl $2
	jmp isr_common_stub

#  3: Int 3 Exception
isr3:
	cli
	pushl $0
	pushl $3
	jmp isr_common_stub

#  4: INTO Exception
isr4:
	cli
	pushl $0
	pushl $4
	jmp isr_common_stub

#  5: Out of Bounds Exception
isr5:
	cli
	pushl $0
	pushl $5
	jmp isr_common_stub

#  6: Invalid Opcode Exception
isr6:
	cli
	pushl $0
	pushl $6
	jmp isr_common_stub

#  7: Coprocessor Not Available Exception
isr7:
	cli
	pushl $0
	pushl $7
	jmp isr_common_stub

#  8: Double Fault Exception (With Error Code!)
isr8:
	cli
	pushl $8
	jmp isr_common_stub

#  9: Coprocessor Segment Overrun Exception
isr9:
	cli
	pushl $0
	pushl $9
	jmp isr_common_stub

# 10: Bad TSS Exception (With Error Code!)
isr10:
	cli
	pushl $10
	jmp isr_common_stub

# 11: Segment Not Present Exception (With Error Code!)
isr11:
	cli
	pushl $11
	jmp isr_common_stub

# 12: Stack Fault Exception (With Error Code!)
isr12:
	cli
	pushl $12
	jmp isr_common_stub

# 13: General Protection Fault Exception (With Error Code!)
isr13:
	cli
	pushl $13
	jmp isr_common_stub

# 14: Page Fault Exception (With Error Code!)
isr14:
	cli
	pushl $14
	jmp isr_common_stub

# 15: Reserved Exception
isr15:
	cli
	pushl $0
	pushl $15
	jmp isr_common_stub

# 16: Floating Point Exception
isr16:
	cli
	pushl $0
	pushl $16
	jmp isr_common_stub

# 17: Alignment Check Exception
isr17:
	cli
	pushl $0
	pushl $17
	jmp isr_common_stub

# 18: Machine Check Exception
isr18:
	cli
	pushl $0
	pushl $18
	jmp isr_common_stub

# 19: Reserved
isr19:
	cli
	pushl $0
	pushl $19
	jmp isr_common_stub

# 20: Reserved
isr20:
	cli
	pushl $0
	pushl $20
	jmp isr_common_stub

# 21: Reserved
isr21:
	cli
	pushl $0
	pushl $21
	jmp isr_common_stub

# 22: Reserved
isr22:
	cli
	pushl $0
	pushl $22
	jmp isr_common_stub

# 23: Reserved
isr23:
	cli
	pushl $0
	pushl $23
	jmp isr_common_stub

# 24: Reserved
isr24:
	cli
	pushl $0
	pushl $24
	jmp isr_common_stub

# 25: Reserved
isr25:
	cli
	pushl $0
	pushl $25
	jmp isr_common_stub

# 26: Reserved
isr26:
	cli
	pushl $0
	pushl $26
	jmp isr_common_stub

# 27: Reserved
isr27:
	cli
	pushl $0
	pushl $27
	jmp isr_common_stub

# 28: Reserved
isr28:
	cli
	pushl $0
	pushl $28
	jmp isr_common_stub

# 29: Reserved
isr29:
	cli
	pushl $0
	pushl $29
	jmp isr_common_stub

# 30: Reserved
isr30:
	cli
	pushl $0
	pushl $30
	jmp isr_common_stub

# 31: Reserved
isr31:
	cli
	pushl $0
	pushl $31
	jmp isr_common_stub

.global isr80
# 80: syscall
isr80:
    cli
    push  %eax # push syscall number
    push  $80
    jmp syscall_stub


.extern syscall_handler
syscall_stub:

	pusha # pushes all regs ax,cx,...,si,di
	push %ds
	push %es
	push %fs
	push %gs

	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	mov %esp, %eax
	push %eax

	mov %esp, %eax # NOTE: may have to change this if its going from user to kernel, adds ss, ...
	add $72, %eax # 18 * 4 bytes
	push %eax # trying to deference parameters
	
	call syscall_handler

	pop %eax

	pop %eax
	pop %gs
	pop %fs
	pop %es
	pop %ds
	popa

	add $8, %esp
	iret

# This is our common ISR stub. It saves the processor state, sets
# up for kernel mode segments, calls the C-level fault handler,
# and finally restores the stack frame.
.extern isr_handler
isr_common_stub:

	pusha # pushes all regs ax,cx,...,si,di
	pushl %ds
	pushl %es
	pushl %fs
	pushl %gs

	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movl %esp, %eax
	pushl %eax
	
	call isr_handler

	popl %eax
	popl %gs
	popl %fs
	popl %es
	popl %ds
	popa

	addl $8, %esp
	iret

.global irq0
.global irq1
.global irq2
.global irq3
.global irq4
.global irq5
.global irq6
.global irq7
.global irq8
.global irq9
.global irq10
.global irq11
.global irq12
.global irq13
.global irq14
.global irq15

# 32: IRQ0
irq0:
    cli
    push  $0
    push  $32
    jmp irq_common_stub

# 33: IRQ1
irq1:
    cli
    push  $0
    push  $33
    jmp irq_common_stub

# 34: IRQ2
irq2:
    cli
    push  $0
    push  $34
    jmp irq_common_stub

# 35: IRQ3
irq3:
    cli
    push  $0
    push  $35
    jmp irq_common_stub

# 36: IRQ4
irq4:
    cli
    push  $0
    push  $36
    jmp irq_common_stub

# 37: IRQ5
irq5:
    cli
    push  $0
    push  $37
    jmp irq_common_stub

# 38: IRQ6
irq6:
    cli
    push  $0
    push  $38
    jmp irq_common_stub

# 39: IRQ7
irq7:
    cli
    push  $0
    push  $39
    jmp irq_common_stub

# 40: IRQ8
irq8:
    cli
    push  $0
    push  $40
    jmp irq_common_stub

# 41: IRQ9
irq9:
    cli
    push  $0
    push  $41
    jmp irq_common_stub

# 42: IRQ10
irq10:
    cli
    push  $0
    push  $42
    jmp irq_common_stub

# 43: IRQ11
irq11:
    cli
    push  $0
    push  $43
    jmp irq_common_stub

# 44: IRQ12
irq12:
    cli
    push  $0
    push  $44
    jmp irq_common_stub

# 45: IRQ13
irq13:
    cli
    push  $0
    push  $45
    jmp irq_common_stub

# 46: IRQ14
irq14:
    cli
    push  $0
    push  $46
    jmp irq_common_stub

# 47: IRQ15
irq15:
    cli
    push  $0
    push  $47
    jmp irq_common_stub


.extern irq_handler
irq_common_stub:

	pusha # pushes all regs ax,cx,...,si,di
	pushl %ds
	pushl %es
	pushl %fs
	pushl %gs

	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movl %esp, %eax
	pushl %eax
	
	call irq_handler

	popl %eax
	popl %gs
	popl %fs
	popl %es
	popl %ds
	popa

	addl $8, %esp
	iret
