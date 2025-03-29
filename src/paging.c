#include <paging.h>

// Function to create a new page table
page_table_t* create_page_table() {
    page_table_t* page_table = (page_table_t*)allocate_page();
    if (!page_table) {
        return NULL;
    }
    // memset(page_table, 0, sizeof(page_table_t));
    return page_table;
}

int map_page(void* physaddr, void* virtualaddr, unsigned int flags) {

    uint32_t pdindex = (uint32_t)virtualaddr >> 22;
    uint32_t ptindex = (uint32_t)virtualaddr >> 12 & 0x3FF;

    uint32_t *pd = (uint32_t *)0xFFFFF000;
    uint32_t *pt = ((uint32_t*)0xFFC00000) + (0x400 * pdindex); // will be equivalent ot new_table
    
    if (!(pd[pdindex] & PAGE_PRESENT)) {
        page_table_t* new_table = create_page_table();
        if (!new_table) {
            return -1; // Allocation failed
        }
        pd[pdindex] = ((uint32_t)new_table & 0xFFFFF000) | flags | PAGE_PRESENT;
        asm volatile("mov %0, %%cr3" :: "r"(pd[0x3FF])); // flush tlb
        // NOTE: need to pass the virtual address, that loops and points to the page table
        memset((void*)pt, 0, sizeof(page_table_t));
    }
    
    if ((pt[ptindex] & PAGE_PRESENT)) { 
        // what to do if the page already exits
        PANIC("Page already exists");
    }

    pt[ptindex] = ((uint32_t)physaddr) | (flags & 0xFFF) | PAGE_PRESENT; // Present
    asm volatile("mov %0, %%cr3" :: "r"(pd[0x3FF])); // flush tlb
    return 0;
}

void *get_physaddr(void* virtualaddr) {
    uint32_t pdindex = (uint32_t)virtualaddr >> 22;
    uint32_t ptindex = (uint32_t)virtualaddr >> 12 & 0x03FF;

    uint32_t *pd = (uint32_t*)0xFFFFF000;
    if (!(pd[pdindex] & PAGE_PRESENT)) {
        PANIC("Page table not present");
    }

    uint32_t *pt = ((uint32_t*)0xFFC00000) + (0x400 * pdindex);
    if (!(pt[ptindex] & PAGE_PRESENT)) { 
        // what to do if the page already exits
        PANIC("Page not present");
    }

    return (void *)((pt[ptindex] & ~0xFFF) + ((uint32_t)virtualaddr & 0xFFF));
}

// must be using the processes page table for this to function
void load_process(uint32_t* process_memory, size_t process_size, uint32_t base_virtual_address) {
    for (size_t i = 0; i < process_size + PAGE_SIZE; i += PAGE_SIZE) {
        uint32_t physical_page = (uint32_t)allocate_page();
        if (!physical_page) {
            // Handle allocation failure
            return;
        }
        map_page((void*)(base_virtual_address + i * PAGE_SIZE), (void*)physical_page, PAGE_WRITE | PAGE_USER);
        memcpy((void*)physical_page, process_memory + i, PAGE_SIZE);
    }
}

void enable_paging(page_directory_t* page_directory) {
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
