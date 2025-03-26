#ifndef UTIL_H
#define UTIL_H

#include <vga.h>
#include <stdbool.h>

#define ASSERT(exp, msg) do { \
                    (exp) ? 0 : PANIC("Assertion Error: "#exp " is false, "msg); \
                    } while (0) \

#define LINE_STR_HELPER(x) #x
#define LINE_STR(x) LINE_STR_HELPER(x)

#define PUSH_ERROR(msg) strcpy(error_msg, __FILE__":"LINE_STR(__LINE__)": error:" msg)
#define PANIC(msg) panic("\n"__FILE__":"LINE_STR(__LINE__)": panic: " msg)

#define UNUSED(x) (void)x

// assume bitmap is bytes, increases as the 32 bit word
#define SET_BIT(bitmap, bit) ((uint8_t*)bitmap)[bit / 32] |= (0b1 << (bit % 32))
#define CLEAR_BIT(bitmap, bit) ((uint8_t*)bitmap)[bit / 32] &= ~(0b1 << (bit % 32))

/**
 * @brief Structure representing a range of bits in a bitmap.
 */
typedef struct {
    uint32_t start;            ///< Starting bit of the range.
    uint32_t length;           ///< Length of the range in bits.
} BitRange;

void panic(const char* msg);


/**
 * @brief Allocates a range of bits in a bitmap.
 * 
 * @param bitmap The bitmap to allocate from.
 * @param count The number of contiguous bits to allocate.
 * @param capacity The number of bits within the bitmap
 * @param word_align for allocators, ensure that we give everything atleast one 32-bit word
 * @return The allocated bit range.
 */
BitRange alloc_bitrange(uint32_t* bitmap, uint32_t capacity, uint32_t count, bool word_align);

/**
 * @brief Deallocates a range of bits in a bitmap.
 * 
 * @param bitmap The bitmap to deallocate from.
 * @param range The range of bits to deallocate.
 */
void dealloc_bitrange(uint32_t* bitmap, BitRange range);

/**
 * @brief Applies a bit range operation on a bitmap.
 *
 * This function modifies a bitmap by setting or clearing a range of bits
 * specified by the `range` parameter. The operation is determined by the
 * `set` parameter.
 *
 * @param bitmap Pointer to the bitmap to be modified.
 * @param range The range of bits to be set or cleared.
 * @param set If true, the bits in the specified range will be set (1).
 *            If false, the bits in the specified range will be cleared (0).
 */
void apply_bitrange(uint32_t* bitmap, BitRange range, bool set);

#endif // UTIL_H