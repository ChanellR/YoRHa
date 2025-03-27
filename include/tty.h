#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>
#include <util.h>
#include <vga.h>

#define TERMINAL_BUFFER_SIZE 1024
#define RING_BUFFER_CAPACITY 64
#define SPECIAL_FILE_COUNT 1

#define TERMINAL_WIDTH 35 // in characters
#define TERMINAL_HEIGHT 20

typedef struct {
    char tty_buffer[TERMINAL_BUFFER_SIZE];
    size_t index; // what we are currently writing to in the buffer
    size_t base_index; // index from which to print as to not properly shift vertically
    // uint32_t inode_num;
} Terminal;

// keyboard input ring buffer
typedef struct {
    char char_buffer[RING_BUFFER_CAPACITY];
    uint32_t tty_fd;
    size_t in_index;
    size_t out_index;
} KeyboardInputBuffer;

extern KeyboardInputBuffer keyboard_input_buffer;
extern Terminal term;

// void initialize_term(int64_t fd);
void render_terminal();

#endif // TTY_H