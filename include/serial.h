#ifndef SERIAL_H
#define SERIAL_H

#include <asm/cpu_io.h>

#define COM1 0x3f8          // COM1

int init_serial();
int is_transmit_empty();
void write_serial(char a);
int serial_received();
char read_serial();

#endif // SERIAL_H