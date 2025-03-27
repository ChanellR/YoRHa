#ifndef FILE_HANDLERS_H
#define FILE_HANDLERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <fs.h>
#include <tty.h>

// NOTE: This is stubbed
#define STDIN 0
#define STDOUT 0
#define STDERR 0 

typedef uint64_t (*file_handler)(bool, int64_t, const void*, uint32_t); // special file handler
typedef void (*initializer)(int64_t);

typedef struct {
    char filename[32];
    file_handler handler;
    int32_t fd;
    // initializer initialization_func;
} SpecialFile;

// will be called on formatting of the disk, or if they are not guaranteed to be there
uint64_t tty_handler(bool read, int64_t fd, const void* buf, uint32_t count);
void create_system_files();

extern SpecialFile system_files[1];

#endif // FILE_HANDLERS_H