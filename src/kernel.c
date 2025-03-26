#include <stdbool.h>
#include <string.h>

#include <asm/cpu_io.h>
#include <fs.h>
#include <interrupts.h>
#include <ata.h>
#include <util.h>
#include <io.h>

void run_tests(void);
extern uint32_t end_kernel;

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

void main(void) 
{
	// NOTE: This is little endian
	initialize_terminal();
	initalize_file_system(false);
	
	gdt_install();
	idt_install();	
	isrs_install();
	irq_install();
	timer_install();
	keyboard_install();
	enable_interrupts();
	
	run_tests();

	// intialize page allocator
// #define PAGE_SIZE 0x1000 // 4KiB
// 	uint32_t first_page_start = (end_kernel & ~0xFFF) + 0x1000;
	// 2^10 KB, 2^20 MB
	// kprintf("End of kernel: %x\n", &end_kernel); // 0x0020_F7E0, 2^21 2MB and some change

	shutdown();
}

