#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

void buff_to_hexstring(void* src, char* dst, uint32_t count);
void buff_to_binstring(void* src, char* dst, uint32_t count);
uint16_t intlen(int32_t value);
void int_to_string(int value, char* output);
size_t str_count(const char* str, char c);
size_t strlen(const char* str);
void memcpy(void* dst, const void* src, uint32_t count);
void memset(void* dst, uint8_t val, size_t count);
void memmove(void* dst, const void* src, size_t count);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
uint32_t strcat(const char* src, char* dst);

// causes an error because string.h is imported by multiple source files
// void test_funct() {}

#endif // STRING_H