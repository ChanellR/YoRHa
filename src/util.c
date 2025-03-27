#include <util.h>

void panic(const char* msg) {
	kprint(msg);
	asm volatile ("cli; hlt");
}

// for alloc
void apply_bitrange(uint32_t* bitmap, BitRange range, bool set) {
	
	// NOTE: TO MAKE SURE
	if (range.length == 0 && range.start == 0) {
		return;
	}

	uint32_t start_word = range.start / (sizeof(uint32_t) * 8); // 32 bits in uint32_t
	uint8_t start_bit = range.start % 32;
	uint32_t end_word = (range.start + range.length) / (sizeof(uint32_t) * 8); // 32 bits in uint32_t
	uint8_t end_bit = (range.start + range.length) % 32; // not inclusive of this bit
	// kprintf("\nstart_word:%u start_bit:%u end_word:%u end_bit:%u\n", start_word, start_bit, end_word, end_bit);
	for (uint16_t word = start_word; word <= end_word; word++) {
		if (start_word == end_word) {
			// BUG: on 1000_0000, ors with 0x7fwfwfwf
			// kprintf("oring with 0x%x\n", (uint32_t)(((1 << (32 - start_bit)) - 1) ^ ((1 << (32 - end_bit)) - 1)));
			uint32_t left = (start_bit) ? ((1 << (32 - start_bit)) - 1) : ~0;
			uint32_t right = ((1 << (32 - end_bit)) - 1);
			if (set) {
				bitmap[word] |= left ^ right;
			} else {
				bitmap[word] &= ~(left ^ right);
			}
			// kprint("in same\n");
			break;
		}

		uint32_t coeff;
		if (word == start_word) {
			// going from left to right 0 -> 31
			coeff = (start_bit) ? (1 << (32 - start_bit)) - 1: ~0b0;
		} else if (word == end_word) {
			coeff = (end_bit) ? ~((1 << (32 - end_bit)) - 1): 0;
		} else {
			coeff = ~0; // set everything
		}

		if (set) {
			bitmap[word] |= coeff;
		} else {
			bitmap[word] &= ~coeff;
		}
	}
}

BitRange alloc_bitrange(uint32_t* bitmap, uint32_t capacity, uint32_t count, bool word_align) {
	// TODO: make more efficient, use words
    BitRange range = { .start = 0, .length = 0 };
	uint32_t curr_length = 0;
	uint32_t curr_start = 0; // this needs to be the nearest zero behind us
    for (uint32_t i = 0; i < capacity/sizeof(uint32_t); i++) {  // Iterate through bytes
		for (int8_t bit = 31; bit >= 0; bit--) {
			// TODO: implement in assembly
			if (bitmap[i] & (1 << bit)) {
				if (word_align) {
					// NOTE: if word_aligned, make sure curr_start begins at the next multiple of 4
					bit &= ~0b11; // make into a multiple of 4, for 4 byte words
				}
				curr_start = (bit) ? i*32 + (31 - (bit - 1)) : (i+1)*32;
				curr_length = 0;
			} else {
				curr_length++;
			}
			if (curr_length == count) {
				range.start = curr_start;
				range.length = curr_length;
				// allocate range
				apply_bitrange(bitmap, range, true);
				return range;
			}
		}
    }

    return range; // No space found
}

void dealloc_bitrange(uint32_t* bitmap, BitRange range) {
	apply_bitrange(bitmap, range, false);
}
