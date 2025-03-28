#include <file_handlers.h>

RingBuffer keyboard_input_buffer = {0};
RingBuffer serial_port_buffer = {0};

void empty_initiazer(int32_t fd) {
    UNUSED(fd);
}

uint64_t tty_handler(bool read, int64_t fd, const void* buf, uint32_t count) {
    UNUSED(fd);
    uint8_t* small_buf = (uint8_t*)buf;
    // kprintf("yes");
    if (read) {
        // read from keyboard input buffer
        uint32_t read = 0;
        for (size_t byte = 0; byte < count; byte++) {
            if (keyboard_input_buffer.out_index == keyboard_input_buffer.in_index) {
                break;
            }
            small_buf[read] = keyboard_input_buffer.char_buffer[keyboard_input_buffer.out_index];
            keyboard_input_buffer.out_index = (keyboard_input_buffer.out_index + 1) % RING_BUFFER_CAPACITY;
            read++;
        }
        return read;
    } else {
        // write to terminal 
        uint32_t written = 0;
        for (size_t byte = 0; byte < count; byte++) {
            char c = ((uint8_t*)buf)[written++];
            term.tty_buffer[term.index] = c;
            term.index = (term.index + 1) % TERMINAL_BUFFER_SIZE;
        }
        render_terminal();
        return written;
    }    
}

void serial_initializer(int32_t fd) {
    UNUSED(fd);
    init_serial();
}

uint64_t serial_handler(bool read, int64_t fd, const void* buf, uint32_t count) {
    uint8_t* small_buf = (uint8_t*)buf;
    UNUSED(count); UNUSED(fd);
    if (read) {
        if (serial_received()) {
            small_buf[0] = inb(COM1);
            return 1;
        } 
    } else {
        // if (is_transmit_empty() == 0);
        if (is_transmit_empty()) {
            outb(COM1, small_buf[0]);
            return 1;
        }
    }
    return 0;
}

void create_system_files() {

    if (mkdir("/dev") == 1) {
        panic(error_msg); // this should just go through 
    }

    for (size_t file = 0; file < sizeof(system_files) / sizeof(SpecialFile); file++) {
        char whole_name[48] = "/dev/";
        strcat(system_files[file].filename, whole_name + 5);
        ASSERT(create_filetype(whole_name, FILE_TYPE_SPECIAL, false) != -1, "Failure in system files assignment");
    }
}

void open_system_files() {
    for (size_t file = 0; file < sizeof(system_files) / sizeof(SpecialFile); file++) {
        char whole_name[48] = "/dev/";
        strcat(system_files[file].filename, whole_name + 5);
        int32_t fd = open(whole_name);
        if (fd == -1) {
            panic(error_msg);
        }
        system_files[file].fd = fd;
        system_files->initialization_func(fd);
    }
}

SpecialFile system_files[2] = {
    {"tty", tty_handler, empty_initiazer, -1},
    {"ttyS", serial_handler, serial_initializer, -1}
};