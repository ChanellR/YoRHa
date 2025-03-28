#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>
#include <util.h>
#include <vga.h>

#define TERMINAL_BUFFER_SIZE 1024
#define RING_BUFFER_CAPACITY 64

#define TERMINAL_WIDTH 35 // in characters
#define TERMINAL_HEIGHT 20

typedef struct {
    char tty_buffer[TERMINAL_BUFFER_SIZE];
    size_t index; // what we are currently writing to in the buffer
    size_t base_index; // index from which to print as to not properly shift vertically
    // uint32_t inode_num;
} Terminal;

extern Terminal term;

// void initialize_term(int64_t fd);
void render_terminal();

#endif // TTY_H