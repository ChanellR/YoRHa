#include <stdbool.h>
#include <string.h>

#include <asm/cpu_io.h>
#include <fs.h>
#include <interrupts.h>
#include <ata.h>
#include <util.h>
#include <io.h>
#include <tests.h>
#include <alloc.h>

// can have normal Registers struct passing, then in the isr80, we jump, put &r in eax, push, put the pointer 
// to the beginning of the stack before the saving of the registers
// and then use that to dereference a struct with the arguments corresponding to the syscall
inline void syscall(uint32_t i) {
	asm volatile (
		"push %%eax;"
		"push $0;"
		"mov %0, %%eax;"
		"int $80;"
		"pop %%eax;"
		: 
		: "m"(i)
	);
}

void syscall_handler(void* arguments, Registers *r) {
	uint32_t val = *((uint32_t*)arguments);
	kprintf("Value: 0x%x\n", val);
	kprintf("call id: 0x%x\n", r->err_code);
}


// NOTE: This is little endian
void main(void) 
{
	initialize_allocator();
	initialize_terminal();
	initalize_file_system(false);

	// kprintf("Hello\n");
	
	gdt_install();
	idt_install();	
	isrs_install();
	irq_install();
	timer_install();
	keyboard_install();
	
	enable_interrupts();
	
	run_tests();

	shutdown();
}

