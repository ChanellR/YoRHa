#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <util.h>
#include <string.h>

// we will allocate 1KB to begin with
// we will allocate on the granularity of bytes
typedef struct {
	void* base_ptr;
	BitRange range;
	bool utilized;
} AllocEntry;

#define PAGE_SIZE 0x1000 // 4KiB
#define HEAP_SIZE 0x400 // 1 KiB
#define MAX_ALLOCATIONS 256
#define BITMAP_CAPACITY HEAP_SIZE/sizeof(uint32_t) // how many bytes are we allocating
typedef struct {
	void* bottom;
	AllocEntry entries[MAX_ALLOCATIONS];
	uint32_t bitmap[BITMAP_CAPACITY];
	bool active;
} AllocArray;

typedef struct {
	size_t len;
	size_t capacity;
	char* contents;
} String;

#define APPEND(s, c) do { \
	if (s.capacity == 0) { \
		s.contents = kcalloc(1, 2); \
		s.capacity = 2; \
		s.len = 1; \
		s.contents[0] = c;  \
	} else if (s.len == s.capacity) { \
		s.contents = krealloc(s.contents, s.capacity * 2); \
		s.capacity *= 2; \
		s.contents[s.len++] = c;							\
	} else { \
		s.contents[s.len++] = c; \
	}			\
} while (0) \


extern uint32_t end_kernel;

void initialize_allocator();
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t num, size_t size);
void* krealloc(void *ptr, size_t new_size);
String concat(String dst, const char* src);

#endif // ALLOC_H