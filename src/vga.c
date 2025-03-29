#include <vga.h>
#include <string.h>

static inline uint8_t vga_entry_color(enum VGA_COLOR fg, enum VGA_COLOR bg) 
{
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}


static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void move_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;

    // Send the low byte of the position
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    // Send the high byte of the position
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// void hide_cursor() {
//     // Disable cursor by modifying the cursor start register
//     outb(0x3D4, 0x0A);  // Select cursor start register
//     outb(0x3D5, 0x20);  // Disable cursor (bit 5 set to 1)
// }

// NOTE: may be bug with VGA initialization
void initialize_terminal(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xC03FF000; // (uint16_t*) 0xB8000; NOTE
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
	// hide_cursor();
}

void terminal_clear(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void kputc(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        return;
    } else if (c == '\b') {
		// not insert
        terminal_column--;
		terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
		move_cursor(terminal_column, terminal_row);
		return;
	}
    
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_row = 0;
	}
	move_cursor(terminal_column, terminal_row);
}

void kwrite(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		kputc(data[i]);
}

// TODO: make this actually accurate, swap unsigned and signed handling
void kputint(int value) {
	uint16_t len = intlen(value);
	len = (len > 0) ? len : 1;
	char buffer[len];
	int_to_string(value, buffer);
	kwrite(buffer, len);
}

void kprint(const char* s) {
	size_t i = 0;
	while (s[i]) {
		kputc(s[i++]);
	}
}

void kprintf(const char* fmt, ...) {
	va_list argptr;
	va_start (argptr, fmt);
	size_t i = 0;

	char c = fmt[i];
	while (c) {
		if (c == '%') {
			char next = fmt[++i];
			if (!next) {
				break;
			}
			switch (next) {
				case 's': { // string
					kprint(va_arg(argptr, char*)); 
					break;
				}
				case 'c': { // character
					kputc(va_arg(argptr, int));
					break;
				}
				case 'd': { // signed decimal integer
					kputint(va_arg(argptr, int32_t));
					break;
				}
				case 'u': { // unsigned decimal integer
					kputint(va_arg(argptr, uint32_t));
					break;
				}
				case 'x': { // hexadecimal
					// assume 32 bit hex value
					char hex_num[9]; // 8 nums plus prefix
					uint32_t num = va_arg(argptr, uint32_t);
					buff_to_hexstring((void*)(&num), hex_num, 4);
					kwrite(hex_num, 8);
					break;
				}
				case 'b': { // binary
					char bin_string[32]; // 8 nums plus prefix
					uint32_t num = va_arg(argptr, uint32_t);
					buff_to_binstring((void*)(&num), bin_string, 4);
					kwrite(bin_string, 32);
					break;
				}
				// TODO: make this like libc
				// case 'l': { // unsigned long long
				// 	kputlong(va_arg(argptr, uint64_t));
				// 	break;
				// }
				default: {
					kputc(c); kputc(next);
				}
			}
		} else {
			kputc(c);
		}
		c = fmt[++i];
    }

	va_end(argptr);
}
