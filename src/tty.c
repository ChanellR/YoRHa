#include <tty.h>

KeyboardInputBuffer keyboard_input_buffer = {0};
Terminal term = {0};

void initialize_term(int64_t fd) {
    UNUSED(fd);
}

void render_terminal() {

    terminal_clear();

    uint32_t lines = 0;
    // keep track of last TERMINAL_HEIGHT new-line characters
    uint32_t lines_indices[TERMINAL_HEIGHT] = {0}; 
    for (size_t i = term.base_index; i < term.index; i++) {
        if (term.tty_buffer[i] == '\n') {
            lines++;
            lines_indices[lines % TERMINAL_HEIGHT] = i + 1;
        }
    }

    // skips ahead to make sure that it is no more than TERMINAL_HEIGHT lines away
    size_t first_line_starter = lines_indices[(lines + 1) % 10];
    if (lines >= TERMINAL_HEIGHT) {
        term.base_index = first_line_starter;
    }
    
    // count back only so many new lines as we want
    if (term.base_index < term.index) {
        kwrite(term.tty_buffer + term.base_index, term.index - term.base_index);
    } else { 
        kwrite(term.tty_buffer + term.base_index, TERMINAL_BUFFER_SIZE - term.base_index);
        kwrite(term.tty_buffer, term.index);
    }
}