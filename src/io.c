#include <io.h>

uint8_t get_scancode() {
    while ((inb(KEYBOARD_STATUS_PORT) & 1) == 0); // Wait until a key is pressed
    return inb(KEYBOARD_DATA_PORT);
}

const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', // 0x0E: Backspace
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', // 0x1C: Enter
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', // 0x2B: Backslash
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, // 0x39: Space
    /* F1-F12 and other keys omitted for simplicity */
};

bool get_keypress(char* c) {
    uint8_t scancode = get_scancode();
    
    if (scancode & 0x80) {
        // Key release event (ignore for now)
        return false;
    }
    
    *c = scancode_to_ascii[scancode];
    return true;
}


void timer_phase(int hz)
{
    int divisor = 1193180 / hz;       /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

void timer_handler(Registers *r)
{
	static int timer_ticks = 0;
    // /* Increment our 'tick count' */
    // timer_ticks++;

    // /* Every 18 clocks (approximately 1 second), we will
    // *  display a message on the screen */
    // if (timer_ticks % 100 == 0)
    // {
    //     kprintf("One second has passed\n");
    // }
	UNUSED(timer_ticks);
	UNUSED(r);
}

void timer_install()
{
    /* Installs 'timer_handler' to IRQ0 */
	timer_phase(100);
    irq_install_handler(0, timer_handler);

}

void keyboard_handler(Registers *r)
{
	static bool shift_pressed = false;
    unsigned char scancode;
	UNUSED(r);

    /* Read from the keyboard's data buffer */
    scancode = inb(0x60);

    /* If the top bit of the byte we read from the keyboard is
    *  set, that means that a key has just been released */
    if (scancode & 0x80)
    {
		/* You can use this one to see if the user released the
		*  shift, alt, or control keys... */
		// Handle key release events for shift keys
		if (scancode == 0xAA || scancode == 0xB6) { // Left or Right Shift released
			shift_pressed = false;
			return;
		}
    }
    else
    {
        /* Here, a key was just pressed. Please note that if you
        *  hold a key down, you will get repeated key press
        *  interrupts. */
		if (scancode == 0x2A || scancode == 0x36) { // Left or Right Shift released
			shift_pressed = true;
			return;
		}
        /* Just to show you how this works, we simply translate
        *  the keyboard scancode into an ASCII value, and then
        *  display it to the screen. You can get creative and
        *  use some flags to see if a shift is pressed and use a
        *  different layout, or you can add another 128 entries
        *  to the above layout to correspond to 'shift' being
        *  held. If shift is held using the larger lookup table,
        *  you would add 128 to the scancode when you look for it */
		char output_char = scancode_to_ascii[scancode];
		if (shift_pressed && output_char >= 'a' && output_char <= 'z') {
			output_char -= 32; // Convert to uppercase
		}
		kputc(output_char);
    }
}

void keyboard_install() {
	irq_install_handler(1, keyboard_handler);
}
