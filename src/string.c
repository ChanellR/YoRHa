#include <stdint.h>
#include <stddef.h>

// inserts `count` bytes into a string as hex, need twice as many bytes +1 in dst as in src 
void buff_to_hexstring(void* src, char* dst, uint32_t count) {
	static const char nib_to_hex[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};

	// words then bytes
	for (uint32_t word = 0; word < count / 4; word++) {
		for (int8_t byte = 3; byte >= 0; byte--) {
			// dst[word*8 + (3-byte)*2] = nib_to_hex[((uint32_t*)src)[word] >> byte * 8 + 4];
			// dst[word*8 + (3-byte)*2 + 1] = nib_to_hex[(((uint32_t*)src)[word] >> byte * 8) & 0xF];
			uint8_t top = (((uint32_t*)src)[word] >> (byte * 8 + 4)) & 0xF;
			uint8_t bottom = (((uint32_t*)src)[word] >> (byte * 8)) & 0xF;
			// kprintf("word: %u, byte: %u, top %u, bottom %u\n", word, byte, top, bottom);
			*(dst++) = nib_to_hex[top];
			*(dst++) = nib_to_hex[bottom];
		}
	}
	// dst[count*2] = '\0'; // NOTE: does this overflow?
}

void buff_to_binstring(void* src, char* dst, uint32_t count) {
	// words then bytes
	for (uint32_t word = 0; word < count / 4; word++) {
		uint32_t copy = ((uint32_t*)src)[word];
		// NOTE: when counting down, must use signed integer
		for (uint8_t bit = 0; bit < 32; bit++) {
			dst[bit] = (copy & (1 << (31 - bit))) ? '1' : '0';
		}
	}
}


// returns the length of the integer as a string
uint16_t intlen(int32_t value) {
	uint16_t i = (value < 0) ? 1 : 0; // account for '-'
	do {
		i++;
		value /= 10;
	} while (value);
	return i;
}

void int_to_string(int value, char* buffer) {
	uint16_t i = intlen(value);
	if (value < 0) {
		buffer[0] = '-';
	}
	buffer[i] = '\0';
	do {
		char c = (char)('0' + (value % 10));
		buffer[--i] = c;
		value /= 10; 
	} while (value);
}

int str_count(const char* s, char target) {
	int count = 0;
	int i = 0;
	while (s) {
		count += (s[i++] == target) ? 1 : 0;
	}
	return count;
}

// only counts how many characters, UNTIL the null
size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

// copy count bytes from src to dst
void memcpy(void* dst, const void* src, uint32_t count) {
	// NOTE: depending on the orientation of the buffer, 
	// if it is uint32_t etc, it may copy the bytes in the 
	// wrong way
	for (uint32_t byte = 0; byte < count; byte++) {
		((uint8_t*)dst)[byte] = ((uint8_t*)src)[byte];
	}
}

void memset(void* dst, uint8_t val, size_t count) {
	for (size_t byte = 0; byte < count; byte++) {
		((uint8_t*)dst)[byte] = val;
	}
}

/*
    Copies the values of num bytes from the location pointed by source
    to the memory block pointed by destination. Copying takes place as if
    an intermediate buffer were used, allowing the destination and source to overlap.
*/
void* memmove(void* dst, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    if (d < s) {
        // Copy forward
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        // Copy backward
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
	return dst;
}

int strcmp(const char* str1, const char* str2) {
    size_t i = 0;
    while (str1[i] && (str1[i] == str2[i])) {
        i++;
    }
    return str1[i] - str2[i];
}

/*
    Copies the C string pointed by source into the array
    pointed by destination, including the terminating null
    character (and stopping at that point)
    returns destination
*/
char* strcpy(char* destination, const char* source) {
    char* ptr = destination;
    while (*source) {
        *ptr++ = *source++;
    }
    *ptr = '\0';
    return destination;
} 

// returns the number of added characters
uint32_t strcat(const char* src, char* dst) {
	uint32_t len = 0;
	while (src[len]) {
		*dst = src[len];
		dst++;
		len++;
	}
	*dst = '\0';
	return len; // so that you can take over the space with \0
}