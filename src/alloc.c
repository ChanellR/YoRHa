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
	new_entry.base_ptr = global_alloc_state.bottom + (allocation.start / 4); // start is number of bytes
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