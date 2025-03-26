#include <stdbool.h>
#include <string.h>

#include <util.h>
#include <vga.h>
#include <ata.h>


#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// assume bitmap is bytes, increases as the 32 bit word
#define SET_BIT(bitmap, bit) ((uint8_t*)bitmap)[bit / 32] |= (0b1 << (bit % 32))
#define CLEAR_BIT(bitmap, bit) ((uint8_t*)bitmap)[bit / 32] &= ~(0b1 << (bit % 32))

uint8_t get_scancode() {
    while ((inb(KEYBOARD_STATUS_PORT) & 1) == 0); // Wait until a key is pressed
    return inb(KEYBOARD_DATA_PORT);
}

const char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', // 0x0E: Backspace
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', // 0x1C: Enter
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', // 0x2B: Backslash
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, // 0x39: Space
    /* F1-F12 and other keys omitted for simplicity */
};

void panic(const char* msg) {
	kprint(msg);
	asm volatile ("hlt");
}

bool get_keypress(char* c) {
    uint8_t scancode = get_scancode();
    
    if (scancode & 0x80) {
        // Key release event (ignore for now)
        return false;
    }
    
    *c = scancode_to_ascii[scancode];
    return true;
}

bool test_ata_pio(void) {

#define SECTOR_SIZE 512
#define SECTOR_COUNT 8 // size of a block

	uint8_t clear[SECTOR_SIZE * SECTOR_COUNT] = {0};
	ata_write_sectors(0, SECTOR_COUNT, clear);
	ata_read_sectors(0, SECTOR_COUNT, clear);

	for (int j = 0; j < 512 * SECTOR_COUNT; j++) {
		if (clear[j] != 0) {
			// output error
			kprintf("\n[clear] difference detected at byte % : ", j);
			kprintf("expected[%]=0", j);
			kprintf(" result[%]=", j);
			kputc(clear[j]);
			kprintf("\n");
			return false;
		}
	}

	uint8_t expected[SECTOR_SIZE * SECTOR_COUNT] = {0};
	const uint8_t result[SECTOR_SIZE * SECTOR_COUNT] = {0};

	for (int s = 0; s < SECTOR_COUNT; s++) {
		for (int i = 0; i < 512; i++) {
			expected[s * 512 + i] = (uint8_t)'a' + (i + s % 26); // shift by sector
		}
	}

	// writing 2 on both
	ata_write_sectors(0, SECTOR_COUNT, expected);
	ata_read_sectors(0, SECTOR_COUNT, result);

	for (int j = 0; j < 512 * SECTOR_COUNT; j++) {
		if (expected[j] != result[j]) {
			// output error
			kprintf("\ndifference detected at byte % : ", j);
			kprintf("expected[%]=", j);
			kputc(expected[j]);
			kprintf(" result[%]=", j);
			kputc(result[j]);
			kprintf("\n");
			return false;
		}
	}

#undef SECTOR_SIZE
#undef SECTOR_COUNT 

	return true;
}

bool test_intlen(void) {
	bool failing = false;
	failing |= intlen(10) != 2;
	failing |= intlen(-11234) != 6;
	failing |= intlen(0) != 1;
	return !failing;
}

bool test_strcmp(void) {
	bool passing = true;
	passing &= strcmp("abc", "ab") > 0;
	passing &= strcmp("abc", "abc") == 0;
	passing &= strcmp("", "") == 0;
	return passing;
}

// https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf
typedef struct {
	// section the entire disk into blocks
	// dedicate blocks header, directory, data
	// 4KB blocks = 8 sectors
	char format_indicator[16]; // magic we will use to tell if a disk is formatted
	uint64_t disk_size; // bytes in disk
	uint32_t sector_count; // total sectors on disk
	uint32_t block_count; // total formatted blocks
	uint32_t i_bmap_start; // i-bmap block
	uint32_t d_bmap_start; // bitmap related to total block allocation
	uint32_t inode_table_start; // inode table start
	uint32_t used_inodes;
	uint32_t data_start; // data section starting
} FileSystemSuper;

typedef struct {
	char name[32];
	uint8_t file_type; // 0 - dir, 1 - file
	uint32_t data_block_start; // first block, will be contiguous
	uint32_t size; // in bytes
	uint32_t parent_inode_num;
} FileSystemInode;

typedef struct {
	char name[32];
	uint32_t inode_num;
} FileSystemDirEntry;

// #define DIR_FILE_COUNT_MAX (BLOCK_BYTES - 2) / sizeof(FileSystemDirEntry)
#define DIR_FILE_COUNT_MAX (BLOCK_BYTES) / sizeof(FileSystemDirEntry)
typedef struct {
	// NOTE: we can just count according to the size in the inode
	// uint16_t files_contained; // how many elements of the contents array
	// only fits in one block
	FileSystemDirEntry contents[DIR_FILE_COUNT_MAX]; // can cast block buffer as pointer
} FileSystemDirDataBlock;


// number of blocks per
#define SUPER_SIZE 1 
#define INODE_BITMAP_SIZE 1
#define DATA_BITMAP_SIZE 1
#define INODE_TABLE_SIZE 5
#define DATA_REGION_START SUPER_SIZE + INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_TABLE_SIZE
#define DATA_REGION_SIZE 64 - SUPER_SIZE - INODE_BITMAP_SIZE - DATA_BITMAP_SIZE - INODE_TABLE_SIZE

static FileSystemSuper global_super;
static uint32_t global_ibmap[BLOCK_BYTES/4] = {0}; // inode occupation bitmap
static uint32_t global_dbmap[BLOCK_BYTES/4] = {0}; // data block occuption bitmap
// will be updated through the runtime, and periodically synced on disk, perhaps on closing
static FileSystemInode global_inode_table[(BLOCK_BYTES * INODE_TABLE_SIZE) / sizeof(FileSystemInode)];

typedef struct {
	uint32_t start;
	uint32_t length; // exclusive 
} BitRange; 

BitRange alloc_bitrange(uint32_t* bitmap, uint32_t count);

// we will format the disk on new disk
// on startup we will read and store the metadata from it, mount the disk
bool initalize_file_system(bool force_format) {

	// TODO: instead of limiting the size of the directory entry table
	// just make the struct, and then reference different parts of the 
	// pointer as the right values 

	// kprintf("Size of FileSysSuper %", sizeof(FileSysSuper));
	uint8_t buffer[512] = {0};
	ata_read_sectors(0, 1, buffer); // NOTE: doesn't work when I use read block, because it overflows
	
	global_super = *(FileSystemSuper*)buffer;
	if (!force_format && strcmp(global_super.format_indicator, "Yorha")) {
		kprintf("Disk Recognized\n");
		// TODO: load structures
		// kprintf("before inode_bitmap: %b\n", global_ibmap[0]);		
		// kprintf("after inode_bitmap: %b\n", global_ibmap[0]);
		ata_read_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, INODE_BITMAP_SIZE);
		ata_read_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, DATA_BITMAP_SIZE);
		ata_read_blocks(global_super.inode_table_start, (uint8_t*)global_inode_table, INODE_TABLE_SIZE);
		return true;	// disk formatted
	}

	kprintf("Formatting Disk...\n");
	// formatting disk
	
	// fill super
	strcpy("Yorha", global_super.format_indicator);
	global_super.disk_size = ata_get_disk_size();
	global_super.sector_count = global_super.disk_size / SECTOR_BYTES;
	global_super.block_count = 64; // only starting with 64
	global_super.i_bmap_start = SUPER_SIZE; // only 1 block for Super
	global_super.d_bmap_start = global_super.i_bmap_start + INODE_BITMAP_SIZE;
	global_super.inode_table_start = global_super.d_bmap_start + DATA_BITMAP_SIZE;
	global_super.data_start = global_super.inode_table_start + INODE_TABLE_SIZE;
	global_super.used_inodes = 1;
	ata_write_blocks(0, (uint8_t*)&global_super, 1); // unsafe

	// clear occupation bitmaps
	global_ibmap[0] |= 1 << 31;
	// global_dbmap[0] |= 1 << 31;
	alloc_bitrange(global_dbmap, DATA_REGION_START + 1); // NOTE: permanently allocates all blocks used for metadata, and the first
	ata_write_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, 1);
	ata_write_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, 1);

	// TODO: add . and .. to directory

	// create root dir at inode 0
	// will have no contents within it, but it exists
	int16_t inode_table_length = global_super.data_start - global_super.inode_table_start; // in blocks
	FileSystemInode root_inode = {.name = "", .file_type = 0, .size = 0, .data_block_start = global_super.data_start, .parent_inode_num = 0};
	global_inode_table[0] = root_inode;
	ata_write_blocks(global_super.inode_table_start, (uint8_t*)global_inode_table, inode_table_length);

	// write empty directory
	// uint8_t root_data_buffer[BLOCK_BYTES] = {0};
	// FileSystemDirDataBlock* dir_tbl_ptr = (FileSystemDirDataBlock*)root_data_buffer;
	// dir_tbl_ptr->files_contained = 0;
	ata_write_blocks(global_super.data_start, NULL, 1);

	return true;
}

typedef struct {
	char name[32];
	uint32_t inode_num;
	uint64_t read_pos;
	uint64_t write_pos;
	uint32_t index;
} FileDescriptorEntry;

typedef struct {
	uint32_t bitmap[1]; // for allocation of fd's
	FileDescriptorEntry entries[32]; // only 16 per process for now
} FileDescriptorTable;

static FileDescriptorTable global_fd_table = {0}; // clear bitmap

static char error_msg[128] = {0}; // error message buffer for everyone to use

// returns the inode_num corresponding to the directory
int32_t seek_directory(const char* dir_path) {

	// must end and begin with '/' (absolute path to directory)
	char next_dir[32] = ""; // maximum 32 character name
	uint32_t current_inode_num = 0; // root directory
	// char* current_char = dir_path + 1; // skip the '/'
	size_t current_char = 1; // skip the '/'
	kprintf("dir_path: %s\n", dir_path);

	uint8_t current_dir_buf[BLOCK_BYTES];
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	if (dir_path[0] != '/') {
		PANIC("relative indexing not implemented");
	}

	// NOTE: ignoring root, maybe rethink abstraction
	uint16_t char_index = 1;
	while (dir_path[current_char]) {
		if (dir_path[current_char] == '/') { // done with writing next directory name
			next_dir[char_index] = '\0'; // end directory name
			// gather previous dir data
			FileSystemInode dir_inode = global_inode_table[current_inode_num];
			ata_read_blocks(dir_inode.data_block_start, current_dir_buf, 1); // only 1 for now
			// look at the current_inode directory for current_char
			// NOTE: calculating files_contained
			uint32_t files_contained = dir_inode.size / sizeof(FileSystemDirEntry); 
			int32_t file_inode_number = -1;
			// for (uint32_t file = 0; file < dir_ptr->files_contained; file++) {
			for (uint32_t file = 0; file < files_contained; file++) {
				if (strcmp(dir_ptr->contents[file].name, next_dir)) {
					// TODO: must ensure this is a valid inode being used
					file_inode_number = dir_ptr->contents[file].inode_num;
					break;
				}
			}
			if (file_inode_number == -1) {
				PUSH_ERROR("couldn't trace path");
				return -1; // couldn't trace the path
			}
			current_inode_num = file_inode_number;
			char_index = 0;
		} else {
			next_dir[char_index++] = dir_path[current_char];
		}
		current_char++;
	}
	return current_inode_num;
}

void parse_path(const char* path, char* dir_path, char* filename) {

	strcpy(path, dir_path);
	char* path_cursor = path;
	uint32_t last_slash = 0;
	while (*path_cursor) {
		if (*path_cursor == '/') {
			last_slash = path_cursor - path;
		}
		path_cursor++;
	}

	strcpy(dir_path + last_slash + 1, filename); // copy tail
	dir_path[last_slash + 1] = '\0'; // end at slash
}

// for bitmap ranges for allocating multiple blocks

// for alloc
void apply_bitrange(uint32_t* bitmap, BitRange range, bool set) {
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

bool test_apply_bitrange(void) {
	bool passing = true;
	uint32_t bitmap[16] = {0};
	BitRange range = {.start = 30, .length = 2};
	apply_bitrange(bitmap, range, true);
	passing &= bitmap[0] == 0x00000003;
	// kprintf("bitmap[0]=%x\n", bitmap[0]);
	range.start = 0;
	range.length = 1;
	apply_bitrange(bitmap, range, true);
	passing &= bitmap[0] == 0x80000003;
	// kprintf("bitmap[0]=0x%x\n", bitmap[0]);
	range.start = 63;
	range.length = 3;
	apply_bitrange(bitmap, range, true);
	// kprintf("bitmap[1]=0x%x bitmap[2]=0x%x \n", bitmap[1], bitmap[2]);
	passing &= bitmap[1] == 0x00000001 && bitmap[2] == 0xC0000000;
	range.start = 28;
	range.length = 8;
	apply_bitrange(bitmap, range, true);
	// kprintf("bitmap[0]=0x%x bitmap[1]=0x%x \n", bitmap[0], bitmap[1]);
	passing &= bitmap[0] == 0x8000000F && bitmap[1] == 0xF0000001;
	apply_bitrange(bitmap, range, false);
	// kprintf("bitmap[0]=0x%x bitmap[1]=0x%x \n", bitmap[0], bitmap[1]);
	passing &= bitmap[0] == 0x80000000 && bitmap[1] == 0x00000001;

	return passing;
}

// TODO: make more efficient, use words
// finds the first contiguous array of block_count blocks in the bitmap
// count down from the MSB
// assumes bitmap is a block long
BitRange alloc_bitrange(uint32_t* bitmap, uint32_t count) {
    BitRange range = { .start = 0, .length = 0 };
	uint32_t curr_length = 0;
	uint32_t curr_start = 0; // this needs to be the nearest zero behind us
    for (uint32_t i = 0; i < BLOCK_BYTES/4; i++) {  // Iterate through bytes
		for (int8_t bit = 31; bit >= 0; bit--) {
			// TODO: implement in assembly
			if (bitmap[i] & (1 << bit)) {
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

bool test_alloc_bitrange() {
	bool passing = true;
	uint32_t bitmap[2] = {0};
	BitRange result;
	result = alloc_bitrange(bitmap, 2);
	passing &= result.start == 0 && result.length == 2;
	// kprintf("result = {.start = %u, .length = %u}\n", result.start, result.length);
	result = alloc_bitrange(bitmap, 8);
	// kprintf("result = {.start = %u, .length = %u}\n", result.start, result.length);
	passing &= result.start == 2 && result.length == 8;
	result = alloc_bitrange(bitmap, 32);
	// kprintf("result = {.start = %u, .length = %u}\n", result.start, result.length);
	// kprintf("bitmap = %b %b\n", bitmap[0], bitmap[1]);
	passing &= result.start == 10 && result.length == 32;
	
	// dealloc
	result.start = 2;
	result.length = 8;
	dealloc_bitrange(bitmap, result);
	// kprintf("bitmap = %b %b\n", bitmap[0], bitmap[1]);
	result = alloc_bitrange(bitmap, 6);
	// kprintf("result = {.start = %u, .length = %u}\n", result.start, result.length);
	passing &= result.start == 2 && result.length == 6;
	// kprintf("bitmap = %b %b\n", bitmap[0], bitmap[1]);

	return passing;
}

// return -1 if can't find file
// returns the inode corresponding to the file within a given
// directory
int32_t search_dir(uint32_t dir_inode_num, char* filename) {
	
	uint8_t current_dir_buf[BLOCK_BYTES];
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	ata_read_blocks(dir_inode.data_block_start, current_dir_buf, 1); // only 1 for now
	// kprintf("[search] dir_inode_num.start: %u\n", dir_inode.data_block_start);

	// NOTE: Also calculating files_contained
	uint32_t files_contained = dir_inode.size / sizeof(FileSystemDirEntry); 
	// kprintf("files_contained: %u\n", files_contained);
	// look at the current_inode directory for current_char
	for (uint32_t file = 0; file < files_contained; file++) {
		// kprintf("found: %s, looking_for: %s\n", dir_ptr->contents[file].name, filename);
		if (strcmp(dir_ptr->contents[file].name, filename)) {
			// TODO: must ensure this is a valid inode being used
			// kprintf("found\n");
			return dir_ptr->contents[file].inode_num;
		}
	}
	PUSH_ERROR("couldn't trace path");
	return -1;
}

// -- FILE SYSTEM SYSCALLS --

// will automatically open the file
// returns a file descriptor
int64_t create(const char* path) {
	
	// follow path to target directory
	char dir_path[strlen(path)];
	char filename[32];
	parse_path(path, dir_path, filename);
	// kprintf("dir: %s, filename: %s, path: %s\n", dir_path, filename, path);

	// NOTE: for these functions that return signed, check
	int32_t dir_inode_num = seek_directory(dir_path);
	if (dir_inode_num == -1) {
		panic(error_msg);
	}
	// kprintf("dir_inode_num: %d\n", dir_inode_num);
	FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	// kprintf("dir_inode = {.data_block_start = %u }\n", dir_inode.data_block_start);
	
	// TODO: ensure that file doesn't already exist
	if (search_dir(dir_inode_num, filename) != -1) {
		PANIC("can't create file under same name");
		return -1; // can't create file under same name
	}
	
	bool is_dir = path[strlen(path) - 1] == '/';
	FileSystemInode file_inode = {.file_type = (is_dir) ? 0 : 1, .parent_inode_num = dir_inode_num, .size = 0};
	strcpy(filename, file_inode.name);
	
	// allocate inode for file
	// kprintf("before inode_bitmap: %b\n", global_ibmap[0]);
	BitRange range = alloc_bitrange(global_ibmap, 1);
	// kprintf("after inode_bitmap: %b\n", global_ibmap[0]);
	if (range.length == 0) { // can't allocate inode
		// shouldn't need to dealloc
		PANIC("can't allocate inode");
		return -1;
	}
	uint32_t file_inode_num = range.start;
	// kprintf("inode_num: %u\n", file_inode_num);
	
	// allocate data blocks for file
	range = alloc_bitrange(global_dbmap, 1);
	if (range.length == 0) {
		PANIC("can't allocate data blocks");
		return -1;
	}
	file_inode.data_block_start = range.start;
	
	// finally add into inodes
	global_inode_table[file_inode_num] = file_inode;
	global_super.used_inodes++;
	
	// add file to directory
	uint8_t current_dir_buf[BLOCK_BYTES];
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	// NOTE: we copy dir_inode here from the global table, instead of as reference, 
	// there may be some bugs where we don't write anything, but we've only been reading
	// so far until increasing size so this may be ok
	ata_read_blocks(dir_inode.data_block_start, current_dir_buf, 1); // NOTE: only 1 for now
	FileSystemDirEntry* new_entry = &(dir_ptr->contents[dir_inode.size / sizeof(FileSystemDirEntry)]);
	new_entry->inode_num = file_inode_num;
	strcpy(filename, new_entry->name);
	dir_inode.size += sizeof(FileSystemDirEntry);
	// kprintf("dir_inode = { .name: %s, .data_block_start: %u }\n", dir_inode.name, dir_inode.data_block_start);
	ata_write_blocks(dir_inode.data_block_start, current_dir_buf, 1); // NOTE: this can break things, if data_block_start is incorrect
	global_inode_table[dir_inode_num] = dir_inode;
	
	// now we should allocate a file descriptor
	range = alloc_bitrange(global_fd_table.bitmap, 1);
	uint32_t fd_index = range.start;
	FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	strcpy(filename, file_descriptor.name);
	global_fd_table.entries[fd_index] = file_descriptor;

	return fd_index; // file descriptor index
}

int64_t open(const char* path) {
	// this will open up some process related state keeping track of the cursor
	// can't open directories

	// follow path to target directory
	char dir_path[strlen(path)];
	char filename[32];
	parse_path(path, dir_path, filename);

	uint32_t dir_inode_num = seek_directory(dir_path);

	// read directory for files
	int32_t file_inode_num = search_dir(dir_inode_num, filename);
	if (file_inode_num == -1) {
		PUSH_ERROR("file doesn't exist");
		return -1; // file doesn't exist
	} 

	// kprintf("[open] file_inode_num: %u\n", file_inode_num);
	// kprintf("Start: %u\n", global_inode_table[file_inode_num].data_block_start);
	// create file descriptor
	BitRange range = alloc_bitrange(global_fd_table.bitmap, 1);
	uint32_t fd_index = range.start;
	FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	strcpy(filename, file_descriptor.name);
	global_fd_table.entries[fd_index] = file_descriptor;

	return fd_index;
}

// NOTE: should these be signed
int64_t close(int64_t fd) {
	BitRange fd_bitmap_range = {.start = fd, .length = 1};
	dealloc_bitrange(global_fd_table.bitmap, fd_bitmap_range);
	return -1;
}

uint64_t read(int64_t fd, const void* buf, uint32_t count) {
	// get inode from fd table
	FileDescriptorEntry* fd_entry = &global_fd_table.entries[fd]; 
	uint32_t fd_inode_num = fd_entry->inode_num;
	FileSystemInode fd_inode = global_inode_table[fd_inode_num];

	// read data_blocks
	uint8_t data_block_buf[BLOCK_BYTES];
	ata_read_blocks(fd_inode.data_block_start, data_block_buf, 1); // NOTE: only 1 block
	// kprintf("[reading] name: %s, fd_inode->data_block_start: %u\n", fd_inode.name, fd_inode.data_block_start);

	// copy into buf, from cursor position, until cursor == size
	uint32_t bytes_read = 0;
	// kprintf("size: %u\n", fd_inode.size);
	while (fd_entry->read_pos < fd_inode.size && bytes_read < count) {
		*((uint8_t*)buf + bytes_read) = data_block_buf[fd_entry->read_pos++];
		bytes_read++; 
	}
	return bytes_read;
}

uint64_t write(int64_t fd, const void* buf, uint32_t count) {
	// TODO: implement some caching for writes before flushing, staging them all to disk
	// get inode from fd table
	FileDescriptorEntry* fd_entry = &global_fd_table.entries[fd]; 
	uint32_t fd_inode_num = fd_entry->inode_num;
	FileSystemInode* fd_inode = &global_inode_table[fd_inode_num];

	// read data_blocks
	uint8_t data_block_buf[BLOCK_BYTES];
	ata_read_blocks(fd_inode->data_block_start, data_block_buf, 1); // NOTE: only 1 block
	// kprintf("[writing] fd_inode->data_block_start: %u\n", fd_inode->data_block_start);
	// write into data_block_buf and commit
	uint32_t bytes_written = 0;
	while (fd_entry->write_pos < BLOCK_BYTES && bytes_written < count) {
		data_block_buf[fd_entry->write_pos++] = *((uint8_t*)buf + bytes_written);
		bytes_written++; 
		fd_inode->size++;
	}
	ata_write_blocks(fd_inode->data_block_start, data_block_buf, 1); // NOTE: only 1 block
	return bytes_written;
}

int64_t unlink(const char* path) {

	char dir_path[strlen(path)];
	char filename[32];
	parse_path(path, dir_path, filename);
	uint32_t dir_inode_num = seek_directory(dir_path);


	// read directory for files
	int32_t file_inode_num = search_dir(dir_inode_num, filename);
	if (file_inode_num == -1) {
		PUSH_ERROR("file doesn't exist");
		return -1; // file doesn't exist
	} 

	// remove from directory data
	
	// unallocate data blocks
	FileSystemInode file_inode = global_inode_table[file_inode_num];

	// unallocate inode
	
	PANIC("Not Implemented");
	return -1;
}

int64_t seek(int64_t fd, uint64_t pos) {
	// TODO: assert that this fd is allocated
	if (true) {
		global_fd_table.entries[fd].read_pos = pos;
		global_fd_table.entries[fd].write_pos = pos;
	} else {
		return -1;
	}
	return pos;
}

uint32_t mkdir(const char* path) {
	PANIC("Not Implemented");
	return -1;
}

// outputs directories to the buffer
void list_dir(const char* path, char* buf) {
	
	char dir_path[strlen(path) + 1];
	char filename[32];
	kprintf("Path: %s\n", path);
	parse_path(path, dir_path, filename);
	kprintf("Dir Path: %s\n", dir_path);
	kprintf("Filename: %s\n", filename);

	uint32_t dir_inode_num = seek_directory(dir_path);
	uint8_t current_dir_buf[BLOCK_BYTES];
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	ata_read_blocks(dir_inode.data_block_start, current_dir_buf, 1); // only 1 for now
	uint32_t files_contained = dir_inode.size / sizeof(FileSystemDirEntry); 
	for (uint32_t file = 0; file < files_contained; file++) {
		buf += strcat(path, buf);
		buf += strcat(dir_ptr->contents[file].name, buf);
		*(buf++) = '\n';
	}
}

void run_tests(void) {
	kprintf("Running Tests...\n");
	
	// kprintf("test_ata_pio...");
	// kprintf((test_ata_pio()) ? "OK\n" : "FAIL\n");

	kprintf("test_intlen...");
	kprintf((test_intlen()) ? "OK\n" : "FAIL\n");
	
	kprintf("test_strcmp...");
	kprintf((test_strcmp()) ? "OK\n" : "FAIL\n");

	kprintf("test_apply_bitrange...");
	kprintf((test_apply_bitrange()) ? "OK\n" : "FAIL\n");

	kprintf("test_alloc_bitrange...");
	kprintf((test_alloc_bitrange()) ? "OK\n" : "FAIL\n");
	
	kprintf("\n");
}

void shutdown() {
	kprintf("Shutting Down...\n");
	kprintf("Syncing Disk Metadata...\n");
	ata_write_blocks(0, (uint8_t*)&global_super, SUPER_SIZE);
	ata_write_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, INODE_BITMAP_SIZE);
	ata_write_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, DATA_BITMAP_SIZE);
	ata_write_blocks(global_super.inode_table_start, (uint8_t*)global_inode_table, INODE_TABLE_SIZE);
}

extern uint32_t end_kernel;
struct gdt_entry {
	/* Limits */
	unsigned short limit_low;
	/* Segment address */
	unsigned short base_low;
	unsigned char base_middle;
	/* Access modes */
	unsigned char access;
	unsigned char granularity;
	unsigned char base_high;
} __attribute__((packed));

/*
 * GDT pointer
 */
struct gdt_ptr {
	unsigned short limit;
	unsigned int base;
} __attribute__((packed));

struct gdt_entry	gdt[6];
struct gdt_ptr		gp;

/**
 * (ASM) gdt_flush
 * Reloads the segment registers
 */
extern void gdt_flush();

/**
 * Set a GDT descriptor
 *
 * @param num The number for the descriptor to set.
 * @param base Base address
 * @param limit Limit
 * @param access Access permissions
 * @param gran Granularity
 */
void
gdt_set_gate(
		int num,
		unsigned long base,
		unsigned long limit,
		unsigned char access,
		unsigned char gran
		) {
	/* Base Address */
	gdt[num].base_low =		(base & 0xFFFF);
	gdt[num].base_middle =	(base >> 16) & 0xFF;
	gdt[num].base_high =	(base >> 24) & 0xFF;
	/* Limits */
	gdt[num].limit_low =	(limit & 0xFFFF);
	gdt[num].granularity =	(limit >> 16) & 0X0F;
	/* Granularity */
	gdt[num].granularity |= (gran & 0xF0);
	/* Access flags */
	gdt[num].access = access;
}

/*
 * gdt_install
 * Install the kernel's GDTs
 */
void
gdt_install() {
	/* GDT pointer and limits */
	gp.limit = (sizeof(struct gdt_entry) * 5) - 1;
	gp.base = (unsigned int)&gdt;
	/* NULL */
	gdt_set_gate(0, 0, 0, 0, 0);
	/* Code segment */
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	/* Data segment */
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
	/* User code */
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
	/* User data */
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
	// write_tss(5, 0x10, 0x0);
	/* Go go go */
	gdt_flush();
	// tss_flush();
}

/*
 * IDT Entry
 */
struct idt_entry {
	unsigned short base_low;
	unsigned short sel;
	unsigned char zero;
	unsigned char flags;
	unsigned short base_high;
} __attribute__((packed));

/*
 * IDT pointer
 */
struct idt_ptr {
	unsigned short limit;
	uintptr_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_load();

/*
 * idt_set_gate
 * Set an IDT gate
 */
void
idt_set_gate(
		unsigned char num,
		unsigned long base,
		unsigned short sel,
		unsigned char flags
		) {
	idt[num].base_low =		(base & 0xFFFF);
	idt[num].base_high =	(base >> 16) & 0xFFFF;
	idt[num].sel =			sel;
	idt[num].zero =			0;
	idt[num].flags =		flags | 0x60;
}

/*
 * idt_install
 * Install the IDTs
 */
void idt_install() {
	idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
	idtp.base = (uintptr_t)&idt;
	memset(&idt, 0, sizeof(struct idt_entry) * 256);
	idt_load();
}

/* bkerndev - Bran's Kernel Development Tutorial
*  By:   Brandon F. (friesenb@gmail.com)
*  Desc: Interrupt Service Routines installer and exceptions
*
*  Notes: No warranty expressed or implied. Use at own risk. */
// #include <system.h>

/* These are function prototypes for all of the exception
*  handlers: The first 32 entries in the IDT are reserved
*  by Intel, and are designed to service exceptions! */
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

/* This is a very repetitive function... it's not hard, it's
*  just annoying. As you can see, we set the first 32 entries
*  in the IDT to the first 32 ISRs. We can't use a for loop
*  for this, because there is no way to get the function names
*  that correspond to that given entry. We set the access
*  flags to 0x8E. This means that the entry is present, is
*  running in ring 0 (kernel level), and has the lower 5 bits
*  set to the required '14', which is represented by 'E' in
*  hex. */
void isrs_install()
{
    idt_set_gate(0, (unsigned)isr0, 0x08, 0x8E);
    idt_set_gate(1, (unsigned)isr1, 0x08, 0x8E);
    idt_set_gate(2, (unsigned)isr2, 0x08, 0x8E);
    idt_set_gate(3, (unsigned)isr3, 0x08, 0x8E);
    idt_set_gate(4, (unsigned)isr4, 0x08, 0x8E);
    idt_set_gate(5, (unsigned)isr5, 0x08, 0x8E);
    idt_set_gate(6, (unsigned)isr6, 0x08, 0x8E);
    idt_set_gate(7, (unsigned)isr7, 0x08, 0x8E);

    idt_set_gate(8, (unsigned)isr8, 0x08, 0x8E);
    idt_set_gate(9, (unsigned)isr9, 0x08, 0x8E);
    idt_set_gate(10, (unsigned)isr10, 0x08, 0x8E);
    idt_set_gate(11, (unsigned)isr11, 0x08, 0x8E);
    idt_set_gate(12, (unsigned)isr12, 0x08, 0x8E);
    idt_set_gate(13, (unsigned)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned)isr14, 0x08, 0x8E);
    idt_set_gate(15, (unsigned)isr15, 0x08, 0x8E);

    idt_set_gate(16, (unsigned)isr16, 0x08, 0x8E);
    idt_set_gate(17, (unsigned)isr17, 0x08, 0x8E);
    idt_set_gate(18, (unsigned)isr18, 0x08, 0x8E);
    idt_set_gate(19, (unsigned)isr19, 0x08, 0x8E);
    idt_set_gate(20, (unsigned)isr20, 0x08, 0x8E);
    idt_set_gate(21, (unsigned)isr21, 0x08, 0x8E);
    idt_set_gate(22, (unsigned)isr22, 0x08, 0x8E);
    idt_set_gate(23, (unsigned)isr23, 0x08, 0x8E);

    idt_set_gate(24, (unsigned)isr24, 0x08, 0x8E);
    idt_set_gate(25, (unsigned)isr25, 0x08, 0x8E);
    idt_set_gate(26, (unsigned)isr26, 0x08, 0x8E);
    idt_set_gate(27, (unsigned)isr27, 0x08, 0x8E);
    idt_set_gate(28, (unsigned)isr28, 0x08, 0x8E);
    idt_set_gate(29, (unsigned)isr29, 0x08, 0x8E);
    idt_set_gate(30, (unsigned)isr30, 0x08, 0x8E);
    idt_set_gate(31, (unsigned)isr31, 0x08, 0x8E);
}

/* This is a simple string array. It contains the message that
*  corresponds to each and every exception. We get the correct
*  message by accessing like:
*  exception_message[interrupt_number] */
unsigned char *exception_messages[] =
{
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

struct regs
{
	// In this order
	uint32_t gs, fs, es, ds;
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;	
    uint32_t int_no, err_code;
	// not as sure about useresp, and ss,
	// haven't tried going from ring 3 -> 0. 
	uint32_t eip, cs, eflags, useresp, ss; 
}__attribute__((packed));

/* All of our Exception handling Interrupt Service Routines will
*  point to this function. This will tell us what exception has
*  happened! Right now, we simply halt the system by hitting an
*  endless loop. All ISRs disable interrupts while they are being
*  serviced as a 'locking' mechanism to prevent an IRQ from
*  happening and messing up kernel data structures */
void isr_handler(struct regs *r)
{
	kprint("Entered\n");
    if (r->int_no < 32)
    {
        kprintf(exception_messages[r->int_no]);
        kprintf(" Exception. System Halted!\n");
        for (;;);
    }
	
	if (r->int_no == 80) {
		kprintf("System call: %d\n", r->err_code);
	}
}


/* These are own ISRs that point to our special IRQ handler
*  instead of the regular 'fault_handler' function */
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void isr80();

/* This array is actually an array of function pointers. We use
*  this to handle custom IRQ handlers for a given IRQ */
void *irq_routines[16] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* This installs a custom IRQ handler for the given IRQ */
void irq_install_handler(int irq, void (*handler)(struct regs *r))
{
    irq_routines[irq] = handler;
}

/* This clears the handler for a given IRQ */
void irq_uninstall_handler(int irq)
{
    irq_routines[irq] = 0;
}

#define PIC_EOI		0x20		/* End-of-interrupt command code */

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

void PIC_sendEOI(uint8_t irq)
{
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
	
	outb(PIC1_COMMAND,PIC_EOI);
}

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */

   #define ICW1_ICW4	0x01		/* Indicates that ICW4 will be present */
   #define ICW1_SINGLE	0x02		/* Single (cascade) mode */
   #define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
   #define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
   #define ICW1_INIT	0x10		/* Initialization - required! */
   
   #define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
   #define ICW4_AUTO	0x02		/* Auto (normal) EOI */
   #define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
   #define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
   #define ICW4_SFNM	0x10		/* Special fully nested (not) */
   
/*
arguments:
    offset1 - vector offset for master PIC
        vectors on the master become offset1..offset1+7
    offset2 - same for slave PIC: offset2..offset2+7
*/
void irq_remap(int offset1, int offset2)
{
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	
	outb(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	outb(PIC2_DATA, ICW4_8086);

	// Unmask both PICs.
	outb(PIC1_DATA, 0);
	outb(PIC2_DATA, 0);
}

/* We first remap the interrupt controllers, and then we install
*  the appropriate ISRs to the correct entries in the IDT. This
*  is just like installing the exception handlers */
void irq_install()
{
    irq_remap(0x20, 0x28);

    idt_set_gate(32, (unsigned)irq0, 0x08, 0x8E);
    idt_set_gate(33, (unsigned)irq1, 0x08, 0x8E);
    idt_set_gate(34, (unsigned)irq2, 0x08, 0x8E);
    idt_set_gate(35, (unsigned)irq3, 0x08, 0x8E);
    idt_set_gate(36, (unsigned)irq4, 0x08, 0x8E);
    idt_set_gate(37, (unsigned)irq5, 0x08, 0x8E);
    idt_set_gate(38, (unsigned)irq6, 0x08, 0x8E);
    idt_set_gate(39, (unsigned)irq7, 0x08, 0x8E);

    idt_set_gate(40, (unsigned)irq8, 0x08, 0x8E);
    idt_set_gate(41, (unsigned)irq9, 0x08, 0x8E);
    idt_set_gate(42, (unsigned)irq10, 0x08, 0x8E);
    idt_set_gate(43, (unsigned)irq11, 0x08, 0x8E);
    idt_set_gate(44, (unsigned)irq12, 0x08, 0x8E);
    idt_set_gate(45, (unsigned)irq13, 0x08, 0x8E);
    idt_set_gate(46, (unsigned)irq14, 0x08, 0x8E);
    idt_set_gate(47, (unsigned)irq15, 0x08, 0x8E);

	idt_set_gate(80, (unsigned)isr80, 0x08, 0x8E);
}

void pic_disable(void) {
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

/* Each of the IRQ ISRs point to this function, rather than
*  the 'fault_handler' in 'isrs.c'. The IRQ Controllers need
*  to be told when you are done servicing them, so you need
*  to send them an "End of Interrupt" command (0x20). There
*  are two 8259 chips: The first exists at 0x20, the second
*  exists at 0xA0. If the second controller (an IRQ from 8 to
*  15) gets an interrupt, you need to acknowledge the
*  interrupt at BOTH controllers, otherwise, you only send
*  an EOI command to the first controller. If you don't send
*  an EOI, you won't raise any more IRQs */
void irq_handler(struct regs *r)
{
    /* This is a blank function pointer */
    void (*handler)(struct regs *r);

    /* Find out if we have a custom handler to run for this
    *  IRQ, and then finally, run it */
    handler = irq_routines[r->int_no - 32];
    if (handler)
    {
        handler(r);
    }

    /* If the IDT entry that was invoked was greater than 40
    *  (meaning IRQ8 - 15), then we need to send an EOI to
    *  the slave controller */
    if (r->int_no >= 40)
    {
        outb(0xA0, 0x20);
    }

    /* In either case, we need to send an EOI to the master
    *  interrupt controller too */
    outb(0x20, 0x20);
}

void timer_phase(int hz)
{
    int divisor = 1193180 / hz;       /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

/* This will keep track of how many ticks that the system
*  has been running for */
int timer_ticks = 0;

/* Handles the timer. In this case, it's very simple: We
*  increment the 'timer_ticks' variable every time the
*  timer fires. By default, the timer fires 18.222 times
*  per second. Why 18.222Hz? Some engineer at IBM must've
*  been smoking something funky */
void timer_handler(struct regs *r)
{
    // /* Increment our 'tick count' */
    // timer_ticks++;

    // /* Every 18 clocks (approximately 1 second), we will
    // *  display a message on the screen */
    // if (timer_ticks % 100 == 0)
    // {
    //     kprintf("One second has passed\n");
    // }
}

/* Sets up the system clock by installing the timer handler
*  into IRQ0 */
void timer_install()
{
    /* Installs 'timer_handler' to IRQ0 */
	timer_phase(100);
    irq_install_handler(0, timer_handler);
}

/* Handles the keyboard interrupt */
void keyboard_handler(struct regs *r)
{
    unsigned char scancode;
	bool static shift_pressed = false;

    /* Read from the keyboard's data buffer */
    scancode = inb(0x60);

    /* If the top bit of the byte we read from the keyboard is
    *  set, that means that a key has just been released */
    if (scancode & 0x80)
    {
		/* You can use this one to see if the user released the
		*  shift, alt, or control keys... */
		// Handle key release events for shift keys
		if (scancode == 0xAA || scancode == 0xB6) { // Left or Right Shift released
			shift_pressed = false;
			return;
		}
    }
    else
    {
        /* Here, a key was just pressed. Please note that if you
        *  hold a key down, you will get repeated key press
        *  interrupts. */
		if (scancode == 0x2A || scancode == 0x36) { // Left or Right Shift released
			shift_pressed = true;
			return;
		}
        /* Just to show you how this works, we simply translate
        *  the keyboard scancode into an ASCII value, and then
        *  display it to the screen. You can get creative and
        *  use some flags to see if a shift is pressed and use a
        *  different layout, or you can add another 128 entries
        *  to the above layout to correspond to 'shift' being
        *  held. If shift is held using the larger lookup table,
        *  you would add 128 to the scancode when you look for it */
		char output_char = scancode_to_ascii[scancode];
		if (shift_pressed && output_char >= 'a' && output_char <= 'z') {
			output_char -= 32; // Convert to uppercase
		}
		kputc(output_char);
    }
}



// can have normal regs struct passing, then in the isr80, we jump, put &r in eax, push, put the pointer 
// to the beginning of the stack before the saving of the registers
// and then use that to dereference a struct with the arguments corresponding to the syscall
inline void syscall(uint32_t i) {
	asm volatile (
		"push %%eax;"
		"push $0;"
		"mov %0, %%eax;"
		"int $80;"
		"pop %%eax;"
		: 
		: "m"(i)
	);
}

/*
	User will call to execute kernel only codings 
*/
void syscall_handler(void* arguments, struct regs *r) {
	uint32_t val = *((uint32_t*)arguments);
	kprintf("Value: 0x%x\n", val);
	kprintf("call id: 0x%x\n", r->err_code);
}

void main(void) 
{
	// NOTE: This is little endian
	initialize_terminal();
	initalize_file_system(false);
	run_tests();

	gdt_install();
	idt_install();	
	isrs_install();
	irq_install();
	timer_install();
	irq_install_handler(1, keyboard_handler);
	// irq_install_handler(80, isr80);

	// int x = 2;
	// kprintf("GDT: %x\n", &gdt);
	// kprintf("IDT: %x\n", &idt);
	// asm volatile ("sti");

	// asm volatile ("syscall");
	// asm volatile ("push %eax; push $4; mov $2, %eax; int $80; pop %eax");
	// syscall(0x123);
	// while(1) {}
	// kprintf("%d\n", x / 0);


	char files[64];
	list_dir("/", files);
	kprintf("Listing files in root dir:\n%s", files);


	// intialize page allocator
// #define PAGE_SIZE 0x1000 // 4KiB
// 	uint32_t first_page_start = (end_kernel & ~0xFFF) + 0x1000;
	// 2^10 KB, 2^20 MB
	// kprintf("End of kernel: %x\n", &end_kernel); // 0x0020_F7E0, 2^21 2MB and some change

	shutdown();
}
