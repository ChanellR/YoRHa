#include <tty.h>

KeyboardInputBuffer keyboard_input_buffer = {0};
Terminal term = {0};

void initialize_term(int64_t fd) {
    UNUSED(fd);
}

void render_terminal() {
    terminal_clear();
    kprint(term.tty_buffer);
}