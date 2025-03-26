#include <ata.h>
#include <stdbool.h>
#include <string.h>
#include <util.h>
#include <fs.h>

// Source:
// https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf

char error_msg[128] = {0};

// number of blocks per
#define SUPER_SIZE 1 
#define INODE_BITMAP_SIZE 1
#define DATA_BITMAP_SIZE 1
#define INODE_TABLE_SIZE 5
#define DATA_REGION_START SUPER_SIZE + INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_TABLE_SIZE
#define DATA_REGION_SIZE 64 - SUPER_SIZE - INODE_BITMAP_SIZE - DATA_BITMAP_SIZE - INODE_TABLE_SIZE

// will be updated through the runtime, and periodically synced on disk, perhaps on closing
static FileSystemSuper global_super;
static uint32_t global_ibmap[BLOCK_BYTES/4] = {0}; // inode occupation bitmap
static uint32_t global_dbmap[BLOCK_BYTES/4] = {0}; // data block occuption bitmap
static FileSystemInode global_inode_table[(BLOCK_BYTES * INODE_TABLE_SIZE) / sizeof(FileSystemInode)];

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
	if (!force_format && strcmp(global_super.format_indicator, "Yorha") == 0) {
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
	strcpy(global_super.format_indicator, "Yorha");
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

static FileDescriptorTable global_fd_table = {0}; // clear bitmap

// returns the inode_num corresponding to the directory
int32_t seek_directory(const char* dir_path) {

	// must end and begin with '/' (absolute path to directory)
	char next_dir[32] = ""; // maximum 32 character name
	uint32_t current_inode_num = 0; // root directory
	// char* current_char = dir_path + 1; // skip the '/'
	size_t current_char = 1; // skip the '/'

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
				if (strcmp(dir_ptr->contents[file].name, next_dir) == 0) {
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

	strcpy(dir_path, path);
	size_t i = 0;

	uint32_t last_slash = 0;
	while (path[i]) {
		if (path[i] == '/') {
			last_slash = i;
		}
		i++;
	}

	strcpy(filename, dir_path + last_slash + 1); // copy tail
	dir_path[last_slash + 1] = '\0'; // end at slash
}


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

// return -1 if can't find file
// returns the inode corresponding to the file within a given
// directory
int32_t search_dir(uint32_t dir_inode_num, char* filename) {
	
	uint8_t current_dir_buf[BLOCK_BYTES] = {0};
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
		if (strcmp(dir_ptr->contents[file].name, filename) == 0) {
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
	strcpy(file_inode.name, filename);
	
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
	strcpy(new_entry->name, filename);
	dir_inode.size += sizeof(FileSystemDirEntry);
	// kprintf("dir_inode = { .name: %s, .data_block_start: %u }\n", dir_inode.name, dir_inode.data_block_start);
	ata_write_blocks(dir_inode.data_block_start, current_dir_buf, 1); // NOTE: this can break things, if data_block_start is incorrect
	global_inode_table[dir_inode_num] = dir_inode;
	
	// now we should allocate a file descriptor
	range = alloc_bitrange(global_fd_table.bitmap, 1);
	uint32_t fd_index = range.start;
	FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	strcpy(file_descriptor.name, filename);
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
	strcpy(file_descriptor.name, filename);
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
	uint8_t data_block_buf[BLOCK_BYTES] = {0};
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
	uint8_t data_block_buf[BLOCK_BYTES] = {0};
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
	// FileSystemInode file_inode = global_inode_table[file_inode_num];

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
	UNUSED(path);
	return -1;
}

// outputs directories to the buffer
void list_dir(const char* path, char* buf) {
	
	char dir_path[strlen(path) + 1];
	char filename[32];
	parse_path(path, dir_path, filename);

	uint32_t dir_inode_num = seek_directory(dir_path);
	kprintf("Dir inode: %x\n", dir_inode_num);
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

void shutdown() {
	// kprintf("Shutting Down...\n");
	// kprintf("Syncing Disk Metadata...\n");
	ata_write_blocks(0, (uint8_t*)&global_super, SUPER_SIZE);
	ata_write_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, INODE_BITMAP_SIZE);
	ata_write_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, DATA_BITMAP_SIZE);
	ata_write_blocks(global_super.inode_table_start, (uint8_t*)global_inode_table, INODE_TABLE_SIZE);
}
