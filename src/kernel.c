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
#include <serial.h>

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

// cmd: ls [dir]
int32_t exec_ls(int32_t stdin, int32_t stdout, StringList cmd) {
	UNUSED(stdin);
	char* ptr = str_list_dir(cmd.contents[1].contents);
	if (!ptr) {
		return -1;
	}
	write(stdout, ptr, strlen(ptr));
	FREE(ptr);
	return 0;
}

// cmd: cat [filename]
int32_t exec_cat(int32_t stdin, int32_t stdout, StringList cmd) {
	UNUSED(stdin);
	char c;
	int32_t fd = open(cmd.contents[1].contents);
	if (fd == -1) {
		return -1;
	}
	while (read(fd, &c, 1) > 0) {
		write(stdout, &c, 1);
	}
	close(fd);
	return 0;
}

// cmd: echo 'text'
int32_t exec_echo(int32_t stdin, int32_t stdout, StringList cmd) {
	UNUSED(stdin);
	uint32_t count = strlen(cmd.contents[1].contents);
	if (write(stdout, cmd.contents[1].contents, count) != count) {
		return -1; // NOTE: Didn't write everything
	}
	return 0;
}

// cmd: touch 'filename'
int32_t exec_touch(int32_t stdin, int32_t stdout, StringList cmd) {
	UNUSED(stdin); UNUSED(stdout);
	return create_filetype(cmd.contents[1].contents, FILE_TYPE_NORMAL, false);
}

// cmd: rm 'filename'
int32_t exec_rm(int32_t stdin, int32_t stdout, StringList cmd) {
	UNUSED(stdin); UNUSED(stdout);
	return unlink(cmd.contents[1].contents);
}

// cmd: mkdir 'filename'
int32_t exec_mkdir(int32_t stdin, int32_t stdout, StringList cmd) {
	UNUSED(stdin); UNUSED(stdout);
	return create_filetype(cmd.contents[1].contents, FILE_TYPE_DIR, false);
}

#define HANDLE(cmd, program) else if (PREFIX(cmd, #program)) { \
	if(exec_program(STDIN, exp1_stdout, cmd) == -1) { \
		write(STDOUT, error_msg, strlen(error_msg)); \
	}; \
}  \

int32_t shell() {
	String curr_command = {.capacity = 0, .contents = 0, .len = 0};
	String working_dir = {0};
	APPEND(working_dir, '/');

	char c;
	write(STDOUT, "$ ", 2);
	while (1) {
		if (read(STDIN, &c, 1) > 0) {
			
			// Delete character
			if (c == '\b') {
				if (curr_command.len > 0) {
					curr_command.len--;
					write(STDOUT, '\b', 1);
				}
				continue;
			}
			
			write(STDOUT, &c, 1);

			// Add character to command
			if (c != '\n') {
				APPEND(curr_command, c);
				continue;
			}

			// Process Command
			if (curr_command.len == 0) {
				write(STDOUT, "\n$ ", 3);
				continue;
			}
			
			APPEND(curr_command, '\0');
			StringList cmd = string_split(curr_command.contents, ' ');

			// look for `>`, then we change fd's
			int32_t exp1_stdout = STDOUT;
			for (size_t i = 0; i < cmd.len - 1; i++) {
				if (MATCH(cmd.contents[i], ">")) {
					ASSERT(i == cmd.len - 2, "only can combo with a single file");
					exp1_stdout = open(cmd.contents[i+1].contents);
					break;
				}
			}

			if (exp1_stdout == -1) {
				write(STDOUT, "Couldn't parse command\n", 24);
				write(STDOUT, "\n$ ", 3);
				continue;
			}

			// [stdin1, stdout2], [stdin2, stdout2]
			int32_t exit_code = 0;
			if (PREFIX(cmd, "exit")) {
				FREE(curr_command); FREE(working_dir); FREE(cmd);
				return 0;
			} else if (PREFIX(cmd, "help")) {
				write(STDOUT, "commands: ls, cat, echo, touch, rm, mkdir\n", 43);
			} else if (PREFIX(cmd, "ls")) {
				exit_code = exec_ls(STDIN, exp1_stdout, cmd);
			} else if (PREFIX(cmd, "cat")) {
				exit_code = exec_cat(STDIN, exp1_stdout, cmd);
			} else if (PREFIX(cmd, "echo")) {
				exit_code = exec_echo(STDIN, exp1_stdout, cmd);
			} else if (PREFIX(cmd, "touch")) {
				exit_code = exec_touch(STDIN, exp1_stdout, cmd);
			} else if (PREFIX(cmd, "rm")) {
				exit_code = exec_rm(STDIN, exp1_stdout, cmd);
			} else if (PREFIX(cmd, "mkdir")) {
				exit_code = exec_mkdir(STDIN, exp1_stdout, cmd);
			} else {
				write(STDOUT, "Couldn't parse command\n", 24);
			}

			if (exp1_stdout != STDOUT) {
				close(exp1_stdout);
			}

			if (exit_code == -1) {
				write(STDERR, error_msg, strlen(error_msg));
			}
			
			FREE(cmd);
			curr_command.len = 0; // reset to nothing
			write(STDOUT, "\n$ ", 3);
		}
	}

	FREE(curr_command); FREE(working_dir);
	return 0;
}

// NOTE: This is little endian
void main(void) 
{
	initialize_allocator();
	initialize_terminal();
	initalize_file_system(true);
	
	gdt_install();
	idt_install();	
	isrs_install();
	irq_install();
	timer_install();
	keyboard_install();
	
	enable_interrupts();
	
	run_tests();

	shell();

	shutdown();
}

