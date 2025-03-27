#include <alloc.h>

static AllocArray global_alloc_state = {0};

void initialize_allocator() {
	global_alloc_state.bottom = (void*)&end_kernel;	
	global_alloc_state.active = true;
}

void* kmalloc(size_t size) {
	ASSERT(global_alloc_state.active, "allocator must be initialized first");
	AllocEntry new_entry;
	BitRange allocation = alloc_bitrange(global_alloc_state.bitmap, BITMAP_CAPACITY, size, true);
	if (allocation.length == 0) {
		PANIC("Insufficient space in heap");
	}
	// NOTE: this may not be going to the right memory address
	new_entry.base_ptr = global_alloc_state.bottom + allocation.start; // start is number of bytes
	new_entry.range = allocation;
	new_entry.utilized = true;
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (!(global_alloc_state.entries[entry].utilized)) {
			global_alloc_state.entries[entry] = new_entry;
			return new_entry.base_ptr;
		}
	}
	PANIC("Maximum allocations reached");
	return NULL;
}

void kfree(void* ptr) {
	ASSERT(global_alloc_state.active, "allocator must be initialized first");
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (global_alloc_state.entries[entry].base_ptr == ptr) {
			global_alloc_state.entries[entry].utilized = false;
			dealloc_bitrange(global_alloc_state.bitmap, global_alloc_state.entries[entry].range);
			return;
		}
	}
	PANIC("Couldn't free ptr");
}

void* kcalloc(size_t num, size_t size) {
	ASSERT(global_alloc_state.active, "allocator must be initialized first");
	void* ptr = kmalloc(num * size);
	memset(ptr, 0, num * size);
	return ptr;
}

void* krealloc(void *ptr, size_t new_size) {
	ASSERT(global_alloc_state.active, "allocator must be initialized first");
	void* new_ptr = kmalloc(new_size);
	for (size_t entry = 0; entry < MAX_ALLOCATIONS; entry++) {
		if (global_alloc_state.entries[entry].base_ptr == ptr) {
			global_alloc_state.entries[entry].utilized = false;
			dealloc_bitrange(global_alloc_state.bitmap, global_alloc_state.entries[entry].range);
			return memmove(new_ptr, ptr, global_alloc_state.entries[entry].range.length); // length will be number of bytes
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

StringList string_split(const char* s, char delim) {
	StringList sl = {.len = 0, .capacity = 0};
	String curr = {0};
	size_t i = 0;
	while (s[i]) {
		if (s[i] == delim) {
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

