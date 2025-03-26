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
.section .multiboot
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

/*
The linker script specifies _start as the entry point to the kernel and the
bootloader will jump to this position once the kernel has been loaded. It
doesn't make sense to return from this function as the bootloader is gone.
*/
.section .text
.global _start
.type _start, @function
_start:
	/*
	The bootloader has loaded us into 32-bit protected mode on a x86
	machine. Interrupts are disabled. Paging is disabled. The processor
	state is as defined in the multiboot standard. The kernel has full
	control of the CPU. The kernel can only make use of hardware features
	and any code it provides as part of itself. There's no printf
	function, unless the kernel provides its own <stdio.h> header and a
	printf implementation. There are no security restrictions, no
	safeguards, no debugging mechanisms, only what the kernel provides
	itself. It has absolute and complete power over the
	machine.
	*/

	/*
	To set up a stack, we set the esp register to point to the top of the
	stack (as it grows downwards on x86 systems). This is necessarily done
	in assembly as languages such as C cannot function without a stack.
	*/
	mov $stack_top, %esp
	
	/*
	This is a good place to initialize crucial processor state before the
	high-level kernel is entered. It's best to minimize the early
	environment where crucial features are offline. Note that the
	processor is not fully initialized yet: Features such as floating
	point instructions and instruction set extensions are not initialized
	yet. The GDT should be loaded here. Paging should be enabled here.
	C++ features such as global constructors and exceptions will require
	runtime support to work as well.
	*/

	/*
	Enter the high-level kernel. The ABI requires the stack is 16-byte
	aligned at the time of the call instruction (which afterwards pushes
	the return pointer of size 4 bytes). The stack was originally 16-byte
	aligned above and we've pushed a multiple of 16 bytes to the
	stack since (pushed 0 bytes so far), so the alignment has thus been
	preserved and the call is well defined.
	*/
	call main

	/*
	If the system has nothing more to do, put the computer into an
	infinite loop. To do that:
	1) Disable interrupts with cli (clear interrupt enable in eflags).
	   They are already disabled by the bootloader, so this is not needed.
	   Mind that you might later enable interrupts and return from
	   kernel_main (which is sort of nonsensical to do).
	2) Wait for the next interrupt to arrive with hlt (halt instruction).
	   Since they are disabled, this will lock up the computer.
	3) Jump to the hlt instruction if it ever wakes up due to a
	   non-maskable interrupt occurring or due to system management mode.
	*/
	cli
1:	hlt
	jmp 1b

/*
Set the size of the _start symbol to the current location '.' minus its start.
This is useful when debugging or when you implement call tracing.
*/
.size _start, . - _start

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
