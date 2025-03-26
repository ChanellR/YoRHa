#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* Hardware text mode color constants. */
enum VGA_COLOR {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum VGA_COLOR fg, enum VGA_COLOR bg) 
{
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void kprintf(const char* fmt, ...);

void initialize_terminal(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_clear(void) {
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
    }
    
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_row = 0;
	}
}

void kwrite(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		kputc(data[i]);
}

// returns the length of the integer as a string
uint16_t intlen(int32_t value) {
	uint16_t i = (value < 0) ? 1 : 0; // account for '-'
	do {
		i++;
		value /= 10;
	} while (value);
	return i;
}

void int_to_string(int value, char* buffer) {
	uint16_t i = intlen(value);
	if (value < 0) {
		buffer[0] = '-';
	}
	buffer[i] = '\0';
	do {
		char c = (char)('0' + (value % 10));
		buffer[--i] = c;
		value /= 10; 
	} while (value);
}

// inserts `count` bytes into a string as hex, need twice as many bytes +1 in dst as in src 
void buff_to_hexstring(void* src, char* dst, uint32_t count) {
	static const char nib_to_hex[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};

	// words then bytes
	for (uint32_t word = 0; word < count / 4; word++) {
		for (int8_t byte = 3; byte >= 0; byte--) {
			// dst[word*8 + (3-byte)*2] = nib_to_hex[((uint32_t*)src)[word] >> byte * 8 + 4];
			// dst[word*8 + (3-byte)*2 + 1] = nib_to_hex[(((uint32_t*)src)[word] >> byte * 8) & 0xF];
			uint8_t top = (((uint32_t*)src)[word] >> (byte * 8 + 4)) & 0xF;
			uint8_t bottom = (((uint32_t*)src)[word] >> (byte * 8)) & 0xF;
			// kprintf("word: %u, byte: %u, top %u, bottom %u\n", word, byte, top, bottom);
			*(dst++) = nib_to_hex[top];
			*(dst++) = nib_to_hex[bottom];
		}
	}
	// dst[count*2] = '\0'; // NOTE: does this overflow?
}

void buff_to_binstring(void* src, char* dst, uint32_t count) {
	// words then bytes
	for (uint32_t word = 0; word < count / 4; word++) {
		uint32_t copy = ((uint32_t*)src)[word];
		// NOTE: when counting down, must use signed integer
		for (uint8_t bit = 0; bit < 32; bit++) {
			dst[bit] = (copy & (1 << (31 - bit))) ? '1' : '0';
		}
	}
}

// TODO: make this actually accurate, swap unsigned and signed handling
void kputint(int value) {
	uint16_t len = intlen(value);
	len = (len > 0) ? len : 1;
	char buffer[len];
	int_to_string(value, buffer);
	kwrite(buffer, len);
}

// void kputlong(uint64_t value) {
// 	uint16_t i = 0; // account for '-'
// 	uint64_t copy = value;
// 	do {
// 		i++;
// 		copy /= 10;
// 	} while (copy);
// 	char buffer[i+1];
// 	buffer[i+1] = '\0';
// 	do {
// 		char c = (char)('0' + (value % 10));
// 		buffer[--i] = c;
// 		value /= 10; 
// 	} while (value);
// }

int str_count(const char* s, char target) {
	int count = 0;
	int i = 0;
	while (s) {
		count += (s[i++] == target) ? 1 : 0;
	}
	return count;
}

void kprint(const char* s) {
	char* base = s;
	while (*s) {
		kputc(*(s++));
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

// copy count bytes from src to dst
void memcpy(void* src, void* dst, uint32_t count) {
	// NOTE: depending on the orientation of the buffer, 
	// if it is uint32_t etc, it may copy the bytes in the 
	// wrong way
	for (uint32_t byte = 0; byte < count; byte++) {
		((uint8_t*)dst)[byte] = ((uint8_t*)src)[byte];
	}
}

void memset(void* dst, uint8_t val, size_t count) {
	for (size_t byte = 0; byte < count; byte++) {
		((uint8_t*)dst)[byte] = val;
	}
}

bool strcomp(char* a, char* b) {
	while (*a || *b) {
		if (*a != *b) {
			return false;
		}
		a++;
		b++;
	}
	return true;
}

// Assumes there is enough space in dst to fit all of src
void str_copy(char* src, char* dst) {
	while (*src) {
		*dst = *src;
		src++;
		dst++;
	}
	*dst = '\0';
}

uint32_t str_len(const char* s) {
	uint32_t len = 0;
	char* cursor = s;
	while (*cursor) {
		cursor++; len++;
	}
	return len;
}

// returns the number of added characters
uint32_t str_concat(char* src, char* dst) {
	uint32_t len = 0;
	while (*src) {
		*dst = *src;
		src++;
		dst++;
		len++;
	}
	*dst = '\0';
	return len; // so that you can take over the space with \0
}

#endif // OUTPUT_H