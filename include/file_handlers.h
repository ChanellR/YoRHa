#ifndef FILE_HANDLERS_H
#define FILE_HANDLERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <fs.h>
#include <tty.h>
#include <serial.h>

// NOTE: This is stubbed
#define STDIN 0
#define STDOUT 0
#define STDERR 0 
#define SERIAL 1

typedef uint64_t (*file_handler)(bool, int64_t, const void*, uint32_t); // special file handler
typedef void (*initializer)(int32_t);

typedef struct {
    char filename[32];
    file_handler handler;
    initializer initialization_func;
    int32_t fd;
} SpecialFile;

typedef struct {
    char char_buffer[RING_BUFFER_CAPACITY];
    uint32_t tty_fd;
    size_t in_index;
    size_t out_index;
} RingBuffer;

extern RingBuffer keyboard_input_buffer;
extern RingBuffer serial_port_buffer;

// will be called on formatting of the disk, or if they are not guaranteed to be there
// uint64_t tty_handler(bool read, int64_t fd, const void* buf, uint32_t count);
void create_system_files();
void open_system_files();

extern SpecialFile system_files[2];

#endif // FILE_HANDLERS_H