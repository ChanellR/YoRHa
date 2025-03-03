#include <vga.h>
#include <ata.h>
#include <stdbool.h>

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
	kprint("PANIC!\n");
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

bool test_strcomp(void) {
	bool passing = true;
	passing &= strcomp("abc", "ab") == false;
	passing &= strcomp("abc", "abc") == true;
	passing &= strcomp("", "") == true;
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
	uint32_t d_bmap_start; // d-bmap block
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

#define INODE_TABLE_SIZE 8-3
static FileSystemSuper global_super;
static uint32_t global_ibmap[BLOCK_BYTES/4] = {0}; // inode occupation bitmap
static uint32_t global_dbmap[BLOCK_BYTES/4] = {0}; // data block occuption bitmap
// will be updated through the runtime, and periodically synced on disk, perhaps on closing
static FileSystemInode global_inode_table[(BLOCK_BYTES * INODE_TABLE_SIZE) / sizeof(FileSystemInode)];

// we will format the disk on new disk
// on startup we will read and store the metadata from it, mount the disk
bool initalize_file_system() {

	// TODO: instead of limiting the size of the directory entry table
	// just make the struct, and then reference different parts of the 
	// pointer as the right values 

	// kprintf("Size of FileSysSuper %", sizeof(FileSysSuper));
	uint8_t buffer[512] = {0};
	ata_read_sectors(0, 1, buffer);

	global_super = *(FileSystemSuper*)buffer;
	if (strcomp(global_super.format_indicator, "Yorha")) {
		kprintf("Disk Recognized\n");
		return true;	// disk formatted
	}

	kprintf("Formatting Disk...\n");
	// formatting disk

	// fill super
	str_copy("Yorha", global_super.format_indicator);
	global_super.disk_size = ata_get_disk_size();
	global_super.sector_count = global_super.disk_size / SECTOR_BYTES;
	global_super.block_count = 64; // only starting with 64
	global_super.i_bmap_start = 1; // only 1 block for Super
	global_super.d_bmap_start = 2;
	global_super.inode_table_start = 3;
	global_super.data_start = 8;
	global_super.used_inodes = 1;
	ata_write_blocks(0, (uint8_t*)&global_super, 1); // unsafe

	// clear occupation bitmaps
	global_ibmap[0] |= 0b1;
	global_dbmap[0] |= 0b1;
	ata_write_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, 1);
	ata_write_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, 1);

	// TODO: add . and .. to directory

	// create root dir at inode 0
	// will have no contents within it, but it exists
	int16_t inode_table_length = global_super.data_start - global_super.inode_table_start; // in blocks
	FileSystemInode root_inode = {.name = "", .file_type = 0, .size = 0, .data_block_start = 0, .parent_inode_num = 0};
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

// -- FILE SYSTEM SYSCALLS --
static char error_msg[64] = {0}; // error message buffer for everyone to use

// returns the inode_num corresponding to the directory
int32_t seek_directory(const char* dir_path) {

	// must end and begin with '/' (absolute path to directory)
	char next_dir[32] = ""; // maximum 32 character name
	uint32_t current_inode_num = 0; // root directory
	char* current_char = dir_path;
	
	// read whole inode_table once
	// uint16_t inode_table_length = global_super.data_start - global_super.inode_table_start; // in blocks
	// uint8_t inode_tbl_buf[BLOCK_BYTES * inode_table_length]; 
	// ata_read_blocks(global_super.inode_table_start, inode_tbl_buf, inode_table_length);

	uint8_t current_dir_buf[BLOCK_BYTES];
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	// NOTE: ignoring root, maybe rethink abstraction
	uint16_t char_index = 1;
	while (*current_char) {
		if (*current_char == '/') { // done with writing next directory name
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
				if (strcomp(dir_ptr->contents[file].name, next_dir)) {
					// TODO: must ensure this is a valid inode being used
					file_inode_number = dir_ptr->contents[file].inode_num;
					break;
				}
			}
			if (file_inode_number == -1) {
				str_copy("Error: couldn't trace path", error_msg);
				return -1; // couldn't trace the path
			}
			current_inode_num = file_inode_number;
			char_index = 0;
		} else {
			next_dir[char_index++] = *current_char;
		}
		current_char++;
	}
	return current_inode_num;
}

void parse_path(const char* path, char* dir_path, char* filename) {

	str_copy(path, dir_path);
	char* path_cursor = path;
	uint32_t last_slash = 0;
	while (*path_cursor) {
		if (*path_cursor == '/') {
			last_slash = path_cursor - path;
		}
		path_cursor++;
	}

	str_copy(dir_path + last_slash + 1, filename); // copy tail
	dir_path[last_slash + 1] = '\0'; // end at slash
}

// for bitmap ranges for allocating multiple blocks
typedef struct {
	uint32_t start;
	uint32_t length; // exclusive 
} BitRange; 

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
	
	// NOTE: Also calculating files_contained
	uint32_t files_contained = dir_inode.size / sizeof(FileSystemDirEntry); 
	// look at the current_inode directory for current_char
	for (uint32_t file = 0; file < files_contained; file++) {
		if (strcomp(dir_ptr->contents[file].name, filename)) {
			// TODO: must ensure this is a valid inode being used
			return dir_ptr->contents[file].inode_num;
		}
	}
	str_copy("Error: couldn't trace path", error_msg);
	return -1;
}

// will automatically open the file
// returns a file descriptor
int64_t create(const char* path) {
	
	// follow path to target directory
	char dir_path[str_len(path)];
	char filename[32];
	parse_path(path, dir_path, filename);

	uint32_t dir_inode_num = seek_directory(dir_path);
	FileSystemInode dir_inode = global_inode_table[dir_inode_num];

	// TODO: ensure that file doesn't already exist
	if (search_dir(dir_inode_num, filename) != -1) {
		str_copy("Error: can't create file under same name", error_msg);
		return -1; // can't create file under same name
	}

	bool is_dir = path[str_len(path) - 1] == '/';
	FileSystemInode file_inode = {.file_type = (is_dir) ? 0 : 1, .parent_inode_num = dir_inode_num, .size = 0};
	str_copy(filename, file_inode.name);

	// allocate inode for file
	BitRange range = alloc_bitrange(global_ibmap, 1);
	if (range.length == 0) { // can't allocate inode
		// shouldn't need to dealloc
		str_copy("Error: can't allocate inode", error_msg);
		return -1;
	}
	uint32_t file_inode_num = range.start;

	// allocate data blocks for file
	range = alloc_bitrange(global_dbmap, 1);
	if (range.length == 0) {
		str_copy("Error: can't allocate data blocks", error_msg);
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
	// FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	ata_read_blocks(dir_inode.data_block_start, current_dir_buf, 1); // NOTE: only 1 for now
	FileSystemDirEntry* new_entry = &(dir_ptr->contents[dir_inode.size / sizeof(FileSystemDirEntry)]);
	new_entry->inode_num = file_inode_num;
	str_copy(filename, new_entry->name);
	dir_inode.size += sizeof(FileSystemDirEntry);
	ata_write_blocks(dir_inode.data_block_start, current_dir_buf, 1);
	global_inode_table[dir_inode_num] = dir_inode;

	// now we should allocate a file descriptor
	range = alloc_bitrange(global_fd_table.bitmap, 1);
	uint32_t fd_index = range.start;
	FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	str_copy(filename, file_descriptor.name);
	global_fd_table.entries[fd_index] = file_descriptor;

	return fd_index; // file descriptor index
}

int64_t open(const char* path) {
	// this will open up some process related state keeping track of the cursor
	// can't open directories

	// follow path to target directory
	char dir_path[str_len(path)];
	char filename[32];
	parse_path(path, dir_path, filename);

	uint32_t dir_inode_num = seek_directory(dir_path);

	// read directory for files
	int32_t file_inode_num = search_dir(dir_inode_num, filename);
	if (file_inode_num == -1) {
		str_copy("Error: file doesn't exist", error_msg);
		return -1; // file doesn't exist
	} 

	// create file descriptor
	BitRange range = alloc_bitrange(global_fd_table.bitmap, 1);
	uint32_t fd_index = range.start;
	FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	str_copy(filename, file_descriptor.name);
	global_fd_table.entries[fd_index] = file_descriptor;

	return -1;
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
	
	// copy into buf, from cursor position, until cursor == size
	uint32_t bytes_read = 0;
	kprintf("size: %u\n", fd_inode.size);
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

int64_t delete(int64_t fd) {
	panic("Error: Not Implemented");
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

void run_tests(void) {
	kprintf("Running Tests...\n");
	
	// kprintf("test_ata_pio...");
	// kprintf((test_ata_pio()) ? "OK\n" : "FAIL\n");

	kprintf("test_intlen...");
	kprintf((test_intlen()) ? "OK\n" : "FAIL\n");
	
	kprintf("test_strcomp...");
	kprintf((test_strcomp()) ? "OK\n" : "FAIL\n");

	kprintf("test_apply_bitrange...");
	kprintf((test_apply_bitrange()) ? "OK\n" : "FAIL\n");

	kprintf("test_alloc_bitrange...");
	kprintf((test_alloc_bitrange()) ? "OK\n" : "FAIL\n");
	
	kprintf("\n");
}

void shutdown() {
	// TODO: resync file system metadata
	// cleanup
}



void main(void) 
{
	
	initialize_terminal();
	initalize_file_system();
	run_tests();

	char* input = "Hello World!";
	char output[13];

	uint32_t fd = create("/hello");
	kprintf("wrote: %u\n", write(fd, input, 13));
	kprintf("read: %u\n", read(fd, output, 13));

	kprintf(output);
	// kprintf("long: %l\n", 12345);

	// kprintf("0x%x\n", 0x10000000);

	// uint32_t byte = 0b00000000;
	// BitRange range = {.start = 2, 3};
	// TODO: write a hex printer

	// asm volatile (
	// 	"bt %2, %1 \n\t"
	// 	"setc %0"
	// 	: "=r" (cf)
	// 	: "m" (byte), "r" (1)
	// );
	// asm volatile (
	// 	"bsr %1, %0"
	// 	: "=r" (index)    // Output: stores bit index
	// 	: "r" (byte)     // Input: value to scan
	// 	: "cc"            // Clobbers: condition codes (ZF flag)
	// );
	// kputint(cf);
	// kputint(index);

	shutdown();
}
