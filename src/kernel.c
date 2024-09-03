#include <vga.h>
#include <ata.h>

void main(void) 
{
	/* Initialize terminal interface */
	terminal_initialize();

	const uint8_t input[512] = "Hello";
	const uint8_t output[512];

	ata_write_sectors(0, 1, input);
	ata_read_sectors(0, 1, output);

	kprintf("Hello\n");
	kprintf("World!");
	
}