#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <util.h>
#include <string.h>
#include <paging.h>

// we will allocate 1KB to begin with
// we will allocate on the granularity of bytes

// #define PAGE_SIZE 0x1000 // 4KiB
#define KERNEL_HEAP_SIZE 0x1000 // 4 KiB
#define MAX_ALLOCATIONS 256

#define KERNEL_BITMAP_CAPACITY KERNEL_HEAP_SIZE / sizeof(uint32_t) // how many bytes are we allocating
#define PAGE_BITMAP_CAPACITY MAX_ALLOCATIONS / sizeof(uint32_t) // how many individual pages are we allocating

#define MATCH(str, cstr) strcmp(str.contents, cstr) == 0
#define PREFIX(cmd, prefix) strcmp(cmd.contents[0].contents, prefix) == 0
#define APPEND(s, c) do { \
	if (s.capacity == 0) { \
		s.contents = kcalloc(2, sizeof(*(s.contents))); \
		s.capacity = 2; \
		s.len = 1; \
		s.contents[0] = c;  \
	} else if (s.len == s.capacity) { \
		s.contents = krealloc(s.contents, sizeof(*(s.contents)) * s.capacity * 2); \
		s.capacity *= 2; \
		s.contents[s.len++] = c;							\
	} else { \
		s.contents[s.len++] = c; \
	}			\
} while (0) \

typedef struct {
	void* base_ptr;
	BitRange range;
	bool utilized;
} AllocEntry;
typedef struct {
	void* bottom;
	AllocEntry entries[MAX_ALLOCATIONS];
	uint32_t bitmap[KERNEL_BITMAP_CAPACITY];
	bool active;
} AllocArray;

typedef struct {
	void* bottom;
	AllocEntry entries[MAX_ALLOCATIONS];
	uint32_t bitmap[PAGE_BITMAP_CAPACITY];
	bool active;
} page_alloc_array_t;

typedef struct {
	size_t len;
	size_t capacity;
	char* contents;
} String;

typedef struct {
	size_t len;
	size_t capacity;
	String* contents;
} StringList;

#define FREE(obj) _Generic((obj), \
    StringList: free_string_list, \
    String: free_string, \
	char*: kfree \
)(obj) 

extern uint32_t end_kernel;

void initialize_allocator();

void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void *ptr, size_t new_size);
void kfree(void* ptr);

String concat(String dst, const char* src);
StringList string_split(const char* s, char delim, bool reserve_quotes);

void free_string_list(StringList sl);
void free_string(String s);

void* allocate_page();
void free_page(void* ptr);

#endif // ALLOC_H