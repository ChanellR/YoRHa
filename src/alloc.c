#include <alloc.h>

/**
 * We need to allocate pages to the OS such that it can allocate memory in that region, I'll just stick with this static setup for now
 */
static AllocArray kernel_allocator = {0};
static page_alloc_array_t page_allocator = {0};

void initialize_allocator() {

	page_allocator.bottom = (void*)0; // start from the beginning
	page_allocator.active = true;

	BitRange kernel_range = alloc_bitrange(page_allocator.bitmap, PAGE_BITMAP_CAPACITY, 1024, false);
	ASSERT(kernel_range.length == 1024 && kernel_range.start == 0, "Should be the same");

	AllocEntry kernel_entry;
	kernel_entry.base_ptr = page_allocator.bottom; // start is number of bytes
	kernel_entry.range = kernel_range;
	kernel_entry.utilized = true;

	ASSERT(!page_allocator.entries[0].utilized, "Shouldn't be currently utilized");
	page_allocator.entries[0] = kernel_entry;

	// TODO: maybe mix these?
	kernel_allocator.bottom = allocate_page(); // NOTE: is this address going to be ok to access
	kernel_allocator.active = true;
	if (map_page(kernel_allocator.bottom, kernel_allocator.bottom, PAGE_WRITE | PAGE_USER) == -1) {
		PANIC("Couldn't allocate heap");
	}
}

void* kmalloc(size_t size) {
	ASSERT(kernel_allocator.active, "allocator must be initialized first");
	AllocEntry new_entry;
	BitRange allocation = alloc_bitrange(kernel_allocator.bitmap, KERNEL_BITMAP_CAPACITY, size, true);
	if (allocation.length == 0) {
		PANIC("Insufficient space in heap");
	}
	// NOTE: this may not be going to the right memory address
	new_entry.base_ptr = kernel_allocator.bottom + allocation.start; // start is number of bytes
	new_entry.range = allocation;
	new_entry.utilized = true;
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (!(kernel_allocator.entries[entry].utilized)) {
			kernel_allocator.entries[entry] = new_entry;
			return new_entry.base_ptr;
		}
	}
	PANIC("Maximum allocations reached");
	return NULL;
}

void kfree(void* ptr) {
	ASSERT(kernel_allocator.active, "allocator must be initialized first");
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (kernel_allocator.entries[entry].base_ptr == ptr) {
			kernel_allocator.entries[entry].utilized = false;
			dealloc_bitrange(kernel_allocator.bitmap, kernel_allocator.entries[entry].range);
			return;
		}
	}
	PANIC("Couldn't free ptr");
}

void* kcalloc(size_t num, size_t size) {
	ASSERT(kernel_allocator.active, "allocator must be initialized first");
	void* ptr = kmalloc(num * size);
	memset(ptr, 0, num * size);
	return ptr;
}

void* krealloc(void *ptr, size_t new_size) {
	ASSERT(kernel_allocator.active, "allocator must be initialized first");
	void* new_ptr = kmalloc(new_size);
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (kernel_allocator.entries[entry].base_ptr == ptr) {
			kernel_allocator.entries[entry].utilized = false;
			dealloc_bitrange(kernel_allocator.bitmap, kernel_allocator.entries[entry].range);
			return memmove(new_ptr, ptr, kernel_allocator.entries[entry].range.length); // length will be number of bytes
		}
	}
	PANIC("Couldn't realloc ptr");
	return NULL;
}

inline void free_string_list(StringList sl) {
	for (size_t i = 0; i < sl.len; i++) {
		kfree(sl.contents->contents); // NOTE: only those that are allocated
	}
	kfree(sl.contents);
}

inline void free_string(String s) {
	kfree(s.contents);
}

String concat(String dst, const char* src) {
	size_t i = 0;
	while (src[i]) {
		// NOTE: This may leak memory, test this
		APPEND(dst, src[i++]);
	}
	return dst;
}

StringList string_split(const char* s, char delim, bool reserve_quotes) {
	StringList sl = {.len = 0, .capacity = 0};
	String curr = {0};
	size_t i = 0;
	bool in_string = false;
	while (s[i]) {

		if (s[i] == '\"') {
			in_string = !in_string;
		}

		if (s[i] == delim && (!in_string || !reserve_quotes)) {
			APPEND(curr, '\0');
			APPEND(sl, curr);
			curr.capacity = 0; // NOTE: allocate a completely new pointer
		} else {
			APPEND(curr, s[i]);
		}
		i++;
	}	
	if (curr.len > 0) {
		// don't free it in the last place, because we free them
		// all at the end
		APPEND(curr, '\0');
		APPEND(sl, curr); 
	}
	return sl;
}

void* allocate_page() {
	ASSERT(page_allocator.active, "allocator must be initialized first");
	AllocEntry new_entry;
	BitRange allocation = alloc_bitrange(page_allocator.bitmap, PAGE_BITMAP_CAPACITY, 1, false);
	if (allocation.length == 0) {
		PANIC("Insufficient space in memory for page allocation");
	}
	// NOTE: this may not be going to the right memory address
	new_entry.base_ptr = page_allocator.bottom + (allocation.start * PAGE_SIZE); // start is number of bytes
	new_entry.range = allocation;
	new_entry.utilized = true;
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (!(page_allocator.entries[entry].utilized)) {
			page_allocator.entries[entry] = new_entry;
			return new_entry.base_ptr;
		}
	}
	PANIC("Maximum allocations reached");
	return NULL;
}

void free_page(void* ptr) {
	ASSERT(page_allocator.active, "allocator must be initialized first");
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (page_allocator.entries[entry].base_ptr == ptr) {
			page_allocator.entries[entry].utilized = false;
			dealloc_bitrange(page_allocator.bitmap, page_allocator.entries[entry].range);
			return;
		}
	}
	PANIC("Couldn't free ptr");
}