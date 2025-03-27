#ifndef IO_H
#define IO_H

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#include <stdint.h>
#include <stdbool.h>
#include <interrupts.h>
#include <util.h>
#include <tty.h>
#include <fs.h>

void timer_install();
void keyboard_install();

#endif // IO_H