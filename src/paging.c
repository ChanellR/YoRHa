#include <paging.h>

page_directory_t kernel_page_directory = {0}; // allocate page tables for kernel
page_table_t kernel_page_table = {0}; // only have 1 table for now, 1024*4096 bytes allowed for kernel

// Function to create a new page directory
page_directory_t* create_page_directory() {
    page_directory_t* page_directory = (page_directory_t*)allocate_page();
    if (!page_directory) {
        return NULL;
    }
    memset(page_directory, 0, sizeof(page_directory_t));
    return page_directory;
}

// Function to create a new page table
page_table_t* create_page_table() {
    page_table_t* page_table = (page_table_t*)allocate_page();
    if (!page_table) {
        return NULL;
    }
    memset(page_table, 0, sizeof(page_table_t));
    return page_table;
}

// Function to map a virtual page to a physical page in the page directory
int map_page(page_directory_t* page_directory, uint32_t virtual_address, uint32_t physical_address, uint32_t flags) {
    uint32_t directory_index = (virtual_address >> 22) & 0x3FF;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;

    // Get or create the page table
    if (!(page_directory->entries[directory_index] & PAGE_PRESENT)) {
        page_table_t* new_table = create_page_table();
        if (!new_table) {
            return -1; // Allocation failed
        }
        // NOTE: new_table should be page aligned anyway
        page_directory->entries[directory_index] = ((uint32_t)new_table & 0xFFFFF000) | flags | PAGE_PRESENT;
    }

    page_table_t* page_table = (page_table_t*)(page_directory->entries[directory_index] & 0xFFFFF000);

    // Map the page
    page_table->entries[table_index] = (physical_address & 0xFFFFF000) | flags | PAGE_PRESENT;

    return 0; // Success
}

// Example usage
void load_process(page_directory_t* page_directory, uint32_t* process_memory, size_t process_size, uint32_t base_virtual_address) {
    for (size_t i = 0; i < process_size + PAGE_SIZE; i += PAGE_SIZE) {
        uint32_t physical_page = (uint32_t)allocate_page();
        if (!physical_page) {
            // Handle allocation failure
            return;
        }
        map_page(page_directory, base_virtual_address + i, physical_page, PAGE_WRITE | PAGE_USER);
        memcpy((void*)physical_page, process_memory + i, PAGE_SIZE);
    }
}

inline void enable_paging(page_directory_t* page_directory) {
    // pretty sure this will crash immediately because we don't have a page table for kernel space
    asm volatile (
        "mov %0, %%eax;" 
        "mov %%eax, %%cr3;" // load the page_directory into the register
        "mov %%cr0, %%eax;"
        "or $0x80000001, %%eax;"  // enable protection mode and paging
        "mov %%eax, %%cr0;"
        : : "m"(page_directory)
    );
}

inline void disable_paging() {
    // pretty sure this will crash immediately because we don't have a page table for kernel space
    asm volatile (
        "mov %%cr0, %%eax;"
        "and $0xEFFFFFFF, %%eax;"  // disable paging
        "mov %%eax, %%cr0;" : :
    );
}
