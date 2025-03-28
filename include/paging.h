#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>
#include <alloc.h>

// Constants
#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024

// Page directory and table entry flags
#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

// to be shared amongst all process to be able to jump to kernel
#define HALF_SPACE_TABLE 768

// Typedefs for page directory and table entries
typedef uint32_t page_entry_t;

// Page directory structure
typedef struct {
    page_entry_t entries[PAGE_ENTRIES];
} page_directory_t;

// Page table structure
typedef struct {
    page_entry_t entries[PAGE_ENTRIES];
} page_table_t;

// Function declarations
page_table_t* create_page_table();
// int map_page(page_directory_t* page_directory, uint32_t virtual_address, uint32_t physical_address, uint32_t flags);
// void load_process(page_directory_t* page_directory, uint32_t* process_memory, size_t process_size, uint32_t base_virtual_address);
int map_page(void* physaddr, void* virtualaddr, unsigned int flags);
void load_process(uint32_t* process_memory, size_t process_size, uint32_t base_virtual_address);
void enable_paging(page_directory_t* page_directory);

#endif // PAGING_H
