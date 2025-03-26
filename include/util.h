#ifndef UTIL_H
#define UTIL_H

#include <vga.h>

void panic(const char* msg);

#define ASSERT(exp, msg) do { \
                    (exp) ? 0 : PANIC("Assertion Error: "#exp " == false, "msg); \
                    } while (0) \

#define LINE_STR_HELPER(x) #x
#define LINE_STR(x) LINE_STR_HELPER(x)

#define PUSH_ERROR(msg) strcpy(error_msg, __FILE__":"LINE_STR(__LINE__)": error:" msg)
#define PANIC(msg) panic("\n"__FILE__":"LINE_STR(__LINE__)": panic: " msg)

#define UNUSED(x) (void)x

// assume bitmap is bytes, increases as the 32 bit word
#define SET_BIT(bitmap, bit) ((uint8_t*)bitmap)[bit / 32] |= (0b1 << (bit % 32))
#define CLEAR_BIT(bitmap, bit) ((uint8_t*)bitmap)[bit / 32] &= ~(0b1 << (bit % 32))

#endif // UTIL_H