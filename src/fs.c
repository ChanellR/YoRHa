#include <ata.h>
#include <stdbool.h>
#include <string.h>
#include <util.h>
#include <fs.h>
#include <file_handlers.h>

// Source:
// https://pages.cs.wisc.edu/~remzi/OSTEP/file-implementation.pdf

char error_msg[128] = {0};

// will be updated through the runtime, and periodically synced on disk, perhaps on closing
static FileSystemSuper global_super;
static uint32_t global_ibmap[BLOCK_BYTES/4] = {0}; // inode occupation bitmap
static uint32_t global_dbmap[BLOCK_BYTES/4] = {0}; // data block occuption bitmap
static FileSystemInode global_inode_table[(BLOCK_BYTES * INODE_TABLE_SIZE) / sizeof(FileSystemInode)];
static FileDescriptorTable global_fd_table = {0}; // clear bitmap

// we will format the disk on new disk
// on startup we will read and store the metadata from it, mount the disk
bool initalize_file_system(bool force_format) {

	// TODO: instead of limiting the size of the directory entry table
	// just make the struct, and then reference different parts of the 
	// pointer as the right values 

	uint8_t buffer[512] = {0};
	ata_read_sectors(0, 1, buffer); // NOTE: doesn't work when I use read block, because it overflows
	
	global_super = *(FileSystemSuper*)buffer;
	if (!force_format && strcmp(global_super.format_indicator, "Yorha") == 0) {
		kprintf("Disk Recognized\n");
		ata_read_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, INODE_BITMAP_SIZE);
		ata_read_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, DATA_BITMAP_SIZE);
		ata_read_blocks(global_super.inode_table_start, (uint8_t*)global_inode_table, INODE_TABLE_SIZE);
		open_system_files();
		return true;	// disk formatted
	}

	// formatting disk
	kprintf("Formatting Disk...\n");
	
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
	alloc_bitrange(global_dbmap, BLOCK_BYTES * 8, DATA_REGION_START + 1, false); // NOTE: permanently allocates all blocks used for metadata, and the first
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
	ata_write_blocks(global_super.data_start, NULL, 1);

	create_system_files();
	open_system_files();
	return true;
}

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
		PUSH_ERROR("relative indexing not implemented");
		return -1;
	}
 
	uint16_t char_index = 0;
	while (dir_path[current_char]) {
		if (dir_path[current_char] == '/' || dir_path[current_char + 1] == '\0') { // done with writing next directory name
			// TODO: conserving the syntax of referencing the directory like `/dir_name` without the last `/`
			// NOTE: ignoring root, maybe rethink abstraction
			if (dir_path[current_char] != '/' && dir_path[current_char + 1] == '\0') {
				next_dir[char_index++] = dir_path[current_char];
			}

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

	if (global_inode_table[current_inode_num].file_type != 0) {
		PUSH_ERROR("file is not a directory");
		return -1;
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

// return -1 if can't find file
// returns the inode corresponding to the file within a given
// directory
int32_t search_dir(uint32_t dir_inode_num, char* filename) {
	
	uint8_t current_dir_buf[BLOCK_BYTES] = {0};
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	ASSERT(dir_inode.file_type == 0, "must be a directory"); 
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

// Remember to free con
ParsedPath Path(const char* path) {
	ParsedPath parsed_path;
	size_t path_length = strlen(path) + 1; // ONLY TO THE NULL
	parsed_path.dir_path = kmalloc(path_length);
	parsed_path.filename = kmalloc(path_length);
	// parse_path(path, parsed_path.dir_path, parsed_path.filename);

	strcpy(parsed_path.dir_path, path);
	size_t i = 0;
	uint32_t last_slash = 0;
	while (path[i]) {
		if (path[i] == '/') {
			last_slash = i;
		}
		i++;
	}

	strcpy(parsed_path.filename, parsed_path.dir_path + last_slash + 1); // copy tail
	parsed_path.dir_path[last_slash + 1] = '\0'; // end at slash

	return parsed_path;
}

// allocate inode for a non-existent file
DirInodePair allocate_inode(ParsedPath parsed_path, uint8_t file_type, bool alloc_data) {

	DirInodePair pair;

	// NOTE: for these functions that return signed, check
	int32_t dir_inode_num = seek_directory(parsed_path.dir_path);
	if (dir_inode_num == -1) {
		pair.valid = false;
		return pair;
		// panic(error_msg);
	}
	pair.dir_inode_num = dir_inode_num; 

	// TODO: ensure that file doesn't already exist
	// FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	if (search_dir(dir_inode_num, parsed_path.filename) != -1) {
		PUSH_ERROR("can't create file under same name");
		pair.valid = false;
		return pair; // can't create file under same name
	}

	// NOTE: we assign a file_inode to a directory as soon as its made
	FileSystemInode file_inode = {.file_type = file_type, .parent_inode_num = dir_inode_num, .size = 0};
	strcpy(file_inode.name, parsed_path.filename);

	// allocate inode for file
	BitRange ib_range = alloc_bitrange(global_ibmap, BLOCK_BYTES * 8, 1, false);
	if (ib_range.length == 0) { // can't allocate inode
		// shouldn't need to dealloc
		PANIC("allocate_inode: can't allocate inode");
		pair.valid = false;
		return pair;
	}
	pair.file_inode_num = ib_range.start;

	if (alloc_data) {
		// allocate data blocks for file
		BitRange db_range = alloc_bitrange(global_dbmap, BLOCK_BYTES * 8, 1, false);
		if (db_range.length == 0) {
			PANIC("can't allocate data blocks");
			dealloc_bitrange(global_ibmap, ib_range);
			pair.valid = false;
			return pair;
		}
		file_inode.data_block_start = db_range.start;
	} else {
		file_inode.data_block_start = 0; // place-holder
	}

	// finally add into inodes
	global_inode_table[pair.file_inode_num] = file_inode;
	global_super.used_inodes++;

	pair.valid = true;
	return pair;
}

// changes both of their state such that the file is within the other directory
int32_t link_file_in_dir(uint32_t dir_inode_num, uint32_t file_inode_num) {

	uint8_t current_dir_buf[BLOCK_BYTES] = {0};
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;

	FileSystemInode* dir_inode = &global_inode_table[dir_inode_num];
	FileSystemInode* file_inode = &global_inode_table[file_inode_num];

	// NOTE: we copy dir_inode here from the global table, instead of as reference, 
	// there may be some bugs where we don't write anything, but we've only been reading
	// so far until increasing size so this may be ok
	ata_read_blocks(dir_inode->data_block_start, current_dir_buf, 1); // NOTE: only 1 for now
	
	FileSystemDirEntry* new_entry = &(dir_ptr->contents[dir_inode->size / sizeof(FileSystemDirEntry)]);
	new_entry->inode_num = file_inode_num;
	strcpy(new_entry->name, file_inode->name);

	dir_inode->size += sizeof(FileSystemDirEntry);
	ata_write_blocks(dir_inode->data_block_start, current_dir_buf, 1); // NOTE: this can break things, if data_block_start is incorrect

	return 0;
}

void unlink_file_in_dir(uint32_t dir_inode_num, uint32_t file_inode_num) {
	uint8_t current_dir_buf[BLOCK_BYTES] = {0};
	FileSystemInode* dir_inode = &global_inode_table[dir_inode_num];
	ata_read_blocks(dir_inode->data_block_start, current_dir_buf, 1); // NOTE: only 1 for now
	FileSystemDirDataBlock* data_block = (FileSystemDirDataBlock*)current_dir_buf;

	// NOTE: should do nothing if the file doesn't exist in the dir
	// shift all the entries back by 1
	for (uint32_t entry = 0; entry < dir_inode->size / sizeof(FileSystemDirEntry); entry++) {
		if(data_block->contents[entry].inode_num == (uint32_t)file_inode_num) {
			while (entry < dir_inode->size / sizeof(FileSystemDirEntry)) {
				data_block->contents[entry] = data_block->contents[entry+1];
				entry++;
			}
			dir_inode->size -= sizeof(FileSystemDirEntry); // reduce the size
			break;
		}
	}
	ata_write_blocks(dir_inode->data_block_start, current_dir_buf, 1); // NOTE: this can break things, if data_block_start is incorrect
}

int32_t allocate_file_descriptor(uint32_t file_inode_num, char* filename) {
	BitRange fd_range = alloc_bitrange(global_fd_table.bitmap, BLOCK_BYTES * 8, 1, false);
	if (fd_range.length == 0) {
		return -1;
	}
	uint32_t fd_index = fd_range.start;
	FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	strcpy(file_descriptor.name, filename);
	global_fd_table.entries[fd_index] = file_descriptor;
	// NOTE: we don't keep track of the number of used file descriptors
	return fd_index;
}

int64_t create_filetype(const char* path, uint8_t file_type, bool allocate_fd) {

	if (path[0] != '/') {
		PUSH_ERROR("Relative addressing unimplmented");
		return -1;
	}
	
	ParsedPath parsed_path = Path(path);
	DirInodePair inode_pair = allocate_inode(parsed_path, file_type, (file_type == 2) ? false : true);
	if (!inode_pair.valid) {
		kfree(parsed_path.dir_path);
		kfree(parsed_path.filename);
		PUSH_ERROR("Couldn't allocate inode");
		return -1;
	}

	link_file_in_dir(inode_pair.dir_inode_num, inode_pair.file_inode_num);
	int32_t fd_index = 0;
	if (allocate_fd) {
		fd_index = allocate_file_descriptor(inode_pair.file_inode_num, parsed_path.filename);
	}
	
	kfree(parsed_path.dir_path);
	kfree(parsed_path.filename);

	// cleanup
	if (fd_index == -1) {
		unlink_file_in_dir(inode_pair.dir_inode_num, inode_pair.file_inode_num);
		BitRange data_range = {.start = global_inode_table[inode_pair.file_inode_num].data_block_start, .length = 1};
		// if {0, 0} range, should do nothing
		dealloc_bitrange(global_dbmap, data_range);
		BitRange inode_range = {.start = inode_pair.valid, .length = 1};
		dealloc_bitrange(global_ibmap, inode_range);
	}
	
	return fd_index; // file descriptor index
}

// -- FILE SYSTEM SYSCALLS --
// will automatically open the file
// returns a file descriptor
int64_t create(const char* path) {
	return create_filetype(path, FILE_TYPE_NORMAL, true);
}

int64_t open(const char* path) {
	// this will open up some process related state keeping track of the cursor
	// can't open directories

	// follow path to target directory
	ParsedPath parsed_path = Path(path);

	// char dir_path[strlen(path)];
	// char filename[32];
	// parse_path(path, dir_path, filename);

	uint32_t dir_inode_num = seek_directory(parsed_path.dir_path);

	// read directory for files
	int32_t file_inode_num = search_dir(dir_inode_num, parsed_path.filename);
	if (file_inode_num == -1) {
		PUSH_ERROR("file doesn't exist");
		kfree(parsed_path.dir_path);
		kfree(parsed_path.filename);
		return -1; // file doesn't exist
	} 

	// kprintf("[open] file_inode_num: %u\n", file_inode_num);
	// kprintf("Start: %u\n", global_inode_table[file_inode_num].data_block_start);
	// create file descriptor
	int32_t fd_index = allocate_file_descriptor(file_inode_num, parsed_path.filename);
	
	kfree(parsed_path.dir_path);
	kfree(parsed_path.filename);

	// BitRange range = alloc_bitrange(global_fd_table.bitmap, BLOCK_BYTES * 8, 1, false);
	// uint32_t fd_index = range.start;
	// FileDescriptorEntry file_descriptor = {.write_pos = 0, .read_pos = 0, .inode_num = file_inode_num, .index = fd_index};
	// strcpy(file_descriptor.name, parsed_path.filename);
	// global_fd_table.entries[fd_index] = file_descriptor;

	return fd_index;
}

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

	// special file
	if (fd_inode.file_type == 0x2) {
		for (size_t file = 0; file < sizeof(system_files) / sizeof(SpecialFile); file++) {
			if (strcmp(system_files[file].filename, fd_inode.name) == 0) {
				file_handler handler = system_files[file].handler;
				return handler(true, fd, buf, count);
			}
		}
		return 0;
	}

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

	// special file
	if (fd_inode->file_type == 0x2) {
		for (size_t file = 0; file < sizeof(system_files) / sizeof(SpecialFile); file++) {
			if (strcmp(system_files[file].filename, fd_inode->name) == 0) {
				file_handler handler = system_files[file].handler;
				return handler(false, fd, buf, count);
			}
		}
		return 0;
	}

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

int32_t seek(int64_t fd, int32_t offset, uint32_t param) {
	// TODO: assert that this fd is allocated
	if (global_fd_table.bitmap[0] & (1 << (31 - fd))) {
		switch (param)
		{
		case SEEK_SET:
			global_fd_table.entries[fd].read_pos = offset;
			global_fd_table.entries[fd].write_pos = offset;
			break;
		case SEEK_CUR:
			global_fd_table.entries[fd].read_pos += offset;
			global_fd_table.entries[fd].write_pos += offset;
			break;
		case SEEK_END:
			global_fd_table.entries[fd].read_pos = global_inode_table[global_fd_table.entries[fd].inode_num].size - offset;
			global_fd_table.entries[fd].write_pos = global_inode_table[global_fd_table.entries[fd].inode_num].size - offset;
			break;
		}
	} else {
		PUSH_ERROR("file descriptor is not allocated"); // TODO: put this everywhere
		return -1;
	}
	return global_fd_table.entries[fd].read_pos; // TODO: figure out what this should really be doing
}

int32_t mkdir(const char* path) {
	// int32_t fd = create_filetype(path, FILE_TYPE_DIR, false);
	// if (fd == -1) {
	// 	panic(error_msg);
	// }
	// close(fd);
	return create_filetype(path, FILE_TYPE_DIR, false);
}

// outputs directories to the buffer
void list_dir(const char* path, char* buf) {
	
	char dir_path[strlen(path) + 1];
	char filename[32];
	parse_path(path, dir_path, filename);

	uint32_t dir_inode_num = seek_directory(dir_path);
	// kprintf("Dir inode: %x\n", dir_inode_num);
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

// outputs directories to the buffer
char* str_list_dir(const char* path) {
	
	// char dir_path[strlen(path) + 1];
	// char filename[32];
	// parse_path(path, dir_path, filename);

	int32_t dir_inode_num = seek_directory(path);
	if (dir_inode_num == -1) {
		return NULL;
	}
	// kprintf("Dir inode: %x\n", dir_inode_num);
	uint8_t current_dir_buf[BLOCK_BYTES];
	FileSystemDirDataBlock* dir_ptr = (FileSystemDirDataBlock*)current_dir_buf;
	
	FileSystemInode dir_inode = global_inode_table[dir_inode_num];
	ata_read_blocks(dir_inode.data_block_start, current_dir_buf, 1); // only 1 for now
	uint32_t files_contained = dir_inode.size / sizeof(FileSystemDirEntry); 
	char* base = kmalloc(files_contained * 32 + 1); // NOTE: arbitrary
	char* buf = base;
	for (uint32_t file = 0; file < files_contained; file++) {
		if (file) {
			*(buf++) = '\n';
		}
		buf += strcat(path, buf);
		buf += strcat(dir_ptr->contents[file].name, buf);
	}
	*(buf++) = '\0';
	return base;
}

int32_t unlink(const char* path) {

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

	unlink_file_in_dir(dir_inode_num, file_inode_num);

	// unallocate data blocks 
	FileSystemInode file_inode = global_inode_table[file_inode_num];
	BitRange range = {.start = file_inode.data_block_start, .length = 1};
	dealloc_bitrange(global_dbmap, range);

	// unallocate inode
	range.start = file_inode_num;
	dealloc_bitrange(global_ibmap, range); // don't need to clear the entry
	
	return 0;
}

void shutdown() {
	kprintf("Shutting Down...\n");
	kprintf("Clearing File Descriptors...\n");

	for (size_t file = 0; file < sizeof(system_files) / sizeof(SpecialFile); file++) {
		close(system_files[file].fd);
    }

	kprintf("Syncing Disk Metadata...\n");
	ata_write_blocks(0, (uint8_t*)&global_super, SUPER_SIZE);
	ata_write_blocks(global_super.i_bmap_start, (uint8_t*)global_ibmap, INODE_BITMAP_SIZE);
	ata_write_blocks(global_super.d_bmap_start, (uint8_t*)global_dbmap, DATA_BITMAP_SIZE);
	ata_write_blocks(global_super.inode_table_start, (uint8_t*)global_inode_table, INODE_TABLE_SIZE);
}
