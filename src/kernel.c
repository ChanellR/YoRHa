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
#include <file_handlers.h>

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

int32_t shell() {
	String curr_command = {.capacity = 0, .contents = 0, .len = 0};
	String working_dir = {0};
	APPEND(working_dir, '/');

	write(STDOUT, "$ ", 2);
	while (1) {
		char c;
		if (read(STDIN, &c, 1) > 0) {
			write(STDOUT, &c, 1);
			if (c == '\n') {
				APPEND(curr_command, '\0');
				
				if (strcmp(curr_command.contents, "ls") == 0) {
					char* ptr = str_list_dir(working_dir.contents);
					write(STDOUT, ptr, strlen(ptr));
					kfree(ptr);
				} else if (strcmp(curr_command.contents, "exit") == 0) {
					kfree(curr_command.contents);
					kfree(working_dir.contents);
					return 0;
				} else {
					write(STDOUT, "Couldn't parse command\n", 24);
				}

				curr_command.len = 0; // reset to nothing
				write(STDOUT, "\n$ ", 3);
			} else {
				APPEND(curr_command, c);
			}
		}

	}

	kfree(curr_command.contents);
	kfree(working_dir.contents);
	return 0;
}

// NOTE: This is little endian
void main(void) 
{
	initialize_allocator();
	initialize_terminal();
	initalize_file_system(true);

	// kprintf("Hello\n");
	
	gdt_install();
	idt_install();	
	isrs_install();
	irq_install();
	timer_install();
	keyboard_install();
	
	enable_interrupts();
	
	// run_tests();
	shell();
	
	shutdown();
}

