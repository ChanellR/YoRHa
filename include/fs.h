#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdbool.h>
#include <flags.h>

// error messages
extern char error_msg[128];

/**
 * @brief Structure representing the superblock of the file system.
 */
typedef struct {
    char format_indicator[16]; ///< Magic identifier for the file system.
    uint64_t disk_size;        ///< Total size of the disk in bytes.
    uint32_t sector_count;     ///< Total number of sectors on the disk.
    uint32_t block_count;      ///< Total number of formatted blocks.
    uint32_t i_bmap_start;     ///< Start block of the inode bitmap.
    uint32_t d_bmap_start;     ///< Start block of the data bitmap.
    uint32_t inode_table_start;///< Start block of the inode table.
    uint32_t used_inodes;      ///< Number of used inodes.
    uint32_t data_start;       ///< Start block of the data region.
} FileSystemSuper;

/**
 * @brief Structure representing an inode in the file system.
 */
typedef struct {
    char name[32];             ///< Name of the file or directory.
    uint8_t file_type;         ///< Type of the file (0 - directory, 1 - file).
    uint32_t data_block_start; ///< Starting block of the file's data.
    uint32_t size;             ///< Size of the file in bytes.
    uint32_t parent_inode_num; ///< Parent inode number.
} FileSystemInode;

/**
 * @brief Structure representing a directory entry.
 */
typedef struct {
    char name[32];             ///< Name of the file or directory.
    uint32_t inode_num;        ///< Inode number of the file or directory.
} FileSystemDirEntry;

#define DIR_FILE_COUNT_MAX (BLOCK_BYTES) / sizeof(FileSystemDirEntry)
/**
 * @file fs.h
 * @brief Defines structures and constants for the file system directory data block.
 *
 * @details
 * - `DIR_FILE_COUNT_MAX`:
 *   Defines the maximum number of directory entries that can fit in a single block.
 *   This is calculated as the block size in bytes (`BLOCK_BYTES`) divided by the size
 *   of a single `FileSystemDirEntry`.
 *
 * - `FileSystemDirDataBlock`:
 *   Represents a data block for a directory in the file system. It contains an array
 *   of `FileSystemDirEntry` structures, which represent the entries in the directory.
 *   The size of the array is determined by `DIR_FILE_COUNT_MAX`, ensuring that the
 *   structure fits within a single block.
 *
 * @note
 * The `contents` array can be directly cast from a block buffer pointer, allowing
 * efficient access to directory entries. The number of files contained in the directory
 * can be inferred from the inode size, so an explicit `files_contained` field is omitted.
 */
typedef struct {
	// NOTE: we can just count according to the size in the inode
	// uint16_t files_contained; // how many elements of the contents array
	// only fits in one block
	FileSystemDirEntry contents[DIR_FILE_COUNT_MAX]; // can cast block buffer as pointer
} FileSystemDirDataBlock;

/**
 * @struct FileDescriptorEntry
 * @brief Represents an entry in the file descriptor table.
 *
 * This structure contains metadata for a file descriptor, including the file's
 * name, inode number, read/write positions, and an index for identification.
 *
 * @var FileDescriptorEntry::name
 * The name of the file (up to 31 characters plus null terminator).
 *
 * @var FileDescriptorEntry::inode_num
 * The inode number associated with the file.
 *
 * @var FileDescriptorEntry::read_pos
 * The current read position within the file.
 *
 * @var FileDescriptorEntry::write_pos
 * The current write position within the file.
 *
 * @var FileDescriptorEntry::index
 * An index used to identify the file descriptor entry.
 */
typedef struct {
    char name[32];
	uint32_t inode_num;
	uint64_t read_pos;
	uint64_t write_pos;
	uint32_t index;
} FileDescriptorEntry;

/**
 * @struct FileDescriptorTable
 * @brief Represents a table of file descriptors for a process.
 * 
 * This structure manages the allocation and storage of file descriptors
 * for a process. It includes a bitmap for tracking allocated descriptors
 * and an array of file descriptor entries.
 *
 * @var FileDescriptorTable::bitmap
 * A bitmap used to track the allocation status of file descriptors.
 * Each bit corresponds to a file descriptor.
 *
 * @var FileDescriptorTable::entries
 * An array of file descriptor entries, with a maximum of 32 entries
 * (only 16 are currently supported per process).
 */
typedef struct {
	uint32_t bitmap[1]; // for allocation of fd's
	FileDescriptorEntry entries[32]; // only 16 per process for now
} FileDescriptorTable;

/**
 * @brief Structure representing a range of bits in a bitmap.
 */
typedef struct {
    uint32_t start;            ///< Starting bit of the range.
    uint32_t length;           ///< Length of the range in bits.
} BitRange;

// error message buffer for everyone to use
// char error_msg[128] = {0};

/**
 * @brief Initializes the file system.
 * 
 * @param force_format If true, forces the disk to be formatted.
 * @return true if the file system is successfully initialized, false otherwise.
 */
bool initalize_file_system(bool force_format);

/**
 * @brief Allocates a range of bits in a bitmap.
 * 
 * @param bitmap The bitmap to allocate from.
 * @param count The number of contiguous bits to allocate.
 * @return The allocated bit range.
 */
BitRange alloc_bitrange(uint32_t* bitmap, uint32_t count);

/**
 * @brief Deallocates a range of bits in a bitmap.
 * 
 * @param bitmap The bitmap to deallocate from.
 * @param range The range of bits to deallocate.
 */
void dealloc_bitrange(uint32_t* bitmap, BitRange range);

/**
 * @brief Creates a new file or directory.
 * 
 * @param path The absolute path of the file or directory to create.
 * @return The file descriptor of the created file, or -1 on failure.
 */
int64_t create(const char* path);

/**
 * @brief Opens an existing file.
 * 
 * @param path The absolute path of the file to open.
 * @return The file descriptor of the opened file, or -1 on failure.
 */
int64_t open(const char* path);

/**
 * @brief Closes an open file.
 * 
 * @param fd The file descriptor of the file to close.
 * @return -1 on failure, or 0 on success.
 */
int64_t close(int64_t fd);

/**
 * @brief Reads data from a file.
 * 
 * @param fd The file descriptor of the file to read from.
 * @param buf The buffer to store the read data.
 * @param count The number of bytes to read.
 * @return The number of bytes read, or 0 if end of file is reached.
 */
uint64_t read(int64_t fd, const void* buf, uint32_t count);

/**
 * @brief Writes data to a file.
 * 
 * @param fd The file descriptor of the file to write to.
 * @param buf The buffer containing the data to write.
 * @param count The number of bytes to write.
 * @return The number of bytes written.
 */
uint64_t write(int64_t fd, const void* buf, uint32_t count);

/**
 * @brief Deletes a file or directory.
 * 
 * @param path The absolute path of the file or directory to delete.
 * @return -1 on failure, or 0 on success.
 */
int64_t unlink(const char* path);

/**
 * @brief Moves the file cursor to a specific position.
 * 
 * @param fd The file descriptor of the file.
 * @param pos The position to move the cursor to.
 * @return The new cursor position, or -1 on failure.
 */
int64_t seek(int64_t fd, uint64_t pos);

/**
 * @brief Creates a new directory.
 * 
 * @param path The absolute path of the directory to create.
 * @return -1 on failure, or 0 on success.
 */
uint32_t mkdir(const char* path);

/**
 * @brief Lists the contents of a directory.
 * 
 * @param path The absolute path of the directory.
 * @param buf The buffer to store the directory listing.
 */
void list_dir(const char* path, char* buf);

/**
 * @brief Gracefully shuts down the system by performing necessary cleanup operations.
 *
 * This function performs the following tasks:
 * 1. Prints a shutdown message to the console.
 * 2. Synchronizes disk metadata by writing critical data structures to disk:
 *    - Writes the superblock to disk.
 *    - Writes the inode bitmap to disk.
 *    - Writes the data bitmap to disk.
 *    - Writes the inode table to disk.
 *
 * It ensures that all metadata changes are saved to persistent storage before the system shuts down.
 */
void shutdown();

/**
 * @brief Applies a bit range operation on a bitmap.
 *
 * This function modifies a bitmap by setting or clearing a range of bits
 * specified by the `range` parameter. The operation is determined by the
 * `set` parameter.
 *
 * @param bitmap Pointer to the bitmap to be modified.
 * @param range The range of bits to be set or cleared.
 * @param set If true, the bits in the specified range will be set (1).
 *            If false, the bits in the specified range will be cleared (0).
 */
void apply_bitrange(uint32_t* bitmap, BitRange range, bool set);

#endif // FS_H