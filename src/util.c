#include <vga.h>

void panic(const char* msg) {
	kprint(msg);
	asm volatile ("hlt");
}