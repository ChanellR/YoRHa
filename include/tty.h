#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>
#include <util.h>
#include <vga.h>

#define TERMINAL_BUFFER_SIZE 512
#define RING_BUFFER_CAPACITY 64
#define SPECIAL_FILE_COUNT 1

typedef struct {
    char tty_buffer[TERMINAL_BUFFER_SIZE];
    size_t index;
    uint32_t inode_num;
    uint32_t vertical_line; // what line in the buffer to start from the top at
    uint32_t top_index; // maybe an alternative, the character we render starting at the top corner
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