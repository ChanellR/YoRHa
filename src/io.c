#include <io.h>


const char scancode_to_ascii[128] = {
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', // 0x0E: Backspace
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', // 0x1C: Enter
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', // 0x2B: Backslash
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, // 0x39: Space
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x3A-0x45: Function keys (F1-F10)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x46-0x55: Extended keys
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 0x56-0x7F: Remaining keys
};

const char scancode_to_ascii_shifted[128] = {
    0,  0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', // 0x0E: Backspace
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', // 0x1C: Enter
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', // 0x2B: Backslash
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, // 0x39: Space
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x3A-0x45: Function keys (F1-F10)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x46-0x55: Extended keys
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 0x56-0x7F: Remaining keys
};

static inline uint8_t get_scancode() {
    while ((inb(KEYBOARD_STATUS_PORT) & 1) == 0); // Wait until a key is pressed
    return inb(KEYBOARD_DATA_PORT);
}

bool get_keypress(char* c) {
    uint8_t scancode = get_scancode();
    if (scancode & 0x80) {
        return false;
    }
    *c = scancode_to_ascii[scancode];
    return true;
}

void timer_phase(int hz) {
    int divisor = 1193180 / hz;       /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

volatile uint64_t timer_counter = 0;
void timer_handler(Registers *r) {
    timer_counter++;
	UNUSED(r);
}

void timer_install() {
	timer_phase(100); // every tick is 10 ms
    irq_install_handler(0, timer_handler);
}

void keyboard_handler(Registers *r) {

	UNUSED(r);
	static bool shift_pressed = false;

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    if (scancode & 0x80) // was a release event
    {
		if (scancode == 0xAA || scancode == 0xB6) { 
			shift_pressed = false;
			return;
		}
    } else {
		if (scancode == 0x2A || scancode == 0x36) { 
			shift_pressed = true;
			return;
		}

		char output_char = (shift_pressed) ? scancode_to_ascii_shifted[scancode] : scancode_to_ascii[scancode];

        // NOTE: blocking until read from write to the input buffer        
        if (output_char) {
            uint32_t next_index = (keyboard_input_buffer.in_index + 1) % RING_BUFFER_CAPACITY;
            if (next_index != keyboard_input_buffer.out_index) {
                keyboard_input_buffer.char_buffer[keyboard_input_buffer.in_index] = output_char;
                keyboard_input_buffer.in_index = next_index;
            }
        }
    }
}

void keyboard_install() {
	irq_install_handler(1, keyboard_handler);
}

void serial_interrupt_handler(Registers* r) {
    UNUSED(r);
    char output_char = read_serial();
    if (output_char) {
        uint32_t next_index = (serial_port_buffer.in_index + 1) % RING_BUFFER_CAPACITY;
        if (next_index != serial_port_buffer.out_index) {
            serial_port_buffer.char_buffer[serial_port_buffer.in_index] = output_char;
            serial_port_buffer.in_index = next_index;
        }
    }
}

void serial_interrupt_install() {
    irq_install_handler(COM1_IRQ, serial_interrupt_handler);
}
