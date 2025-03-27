#include <file_handlers.h>

SpecialFile system_files[1] = {
    {"tty", tty_handler, -1}
};

uint64_t tty_handler(bool read, int64_t fd, const void* buf, uint32_t count) {
    UNUSED(fd);
    uint8_t* small_buf = (uint8_t*)buf;
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
            term.tty_buffer[term.index] = ((uint8_t*)buf)[written++];
            term.index = (term.index + 1) % TERMINAL_BUFFER_SIZE;
        }
        render_terminal();
        return written;
    }    
}

void create_system_files() {
    mkdir("/dev"); // this should just go through 
    for (size_t file = 0; file < sizeof(system_files) / sizeof(SpecialFile); file++) {
        char whole_name[48] = "/dev/";
        strcat(system_files[file].filename, whole_name + 5);

        int32_t fd = create_filetype(whole_name, FILE_TYPE_SPECIAL, true);
        ASSERT(fd != 1, "Failure in system files assignment");
        system_files[file].fd = fd;
    }
}