# YoRHa

YoRHa is a simple operating system kernel written in C and assembly. It is designed as an educational project to explore low-level system programming concepts, including memory management, interrupt handling, and file systems.

## Features

- **Multiboot Compliant**: The kernel is compatible with multiboot loaders like GRUB.
- **Basic File System**: Includes a custom file system implementation with support for creating, reading, writing, and deleting files.
- **Interrupt Handling**: Implements a Global Descriptor Table (GDT), Interrupt Descriptor Table (IDT), and interrupt service routines (ISRs).
- **ATA PIO Driver**: Provides support for reading and writing to disk using ATA PIO mode.
- **VGA Text Mode**: Basic terminal output using VGA text mode.
- **Keyboard Input**: Captures keyboard input using IRQ1.
- **Timer**: Configurable timer using IRQ0.
- **System Calls**: Basic syscall mechanism for kernel-user communication.

## Directory Structure

- **`src/`**: Contains the kernel source code.
    - `kernel.c`: Entry point for the kernel.
    - `fs.c`: File system implementation.
    - `interrupts.c`: Interrupt handling and descriptor tables.
    - `ata.c`: ATA PIO driver for disk operations.
    - `vga.c`: VGA text mode driver.
    - `io.c`: Keyboard and timer drivers.
    - `util.c`: Utility functions.
    - `tests.c`: Unit tests for various components.
    - `boot.s`: Assembly code for bootstrapping the kernel.
- **`include/`**: Header files for the kernel.
    - `fs.h`, `interrupts.h`, `vga.h`, `ata.h`, `io.h`, `util.h`: Declarations for corresponding modules.
    - `asm/`: Inline assembly utilities.
- **`build/`**: Directory for compiled object files and the final kernel binary.
- **`disk/`**: Contains the disk image used for testing.
- **`scripts/`**: Scripts for setting up the development environment.
    - `install_gcc.sh`, `install_gdb.sh`, `install_binutils.sh`: Scripts to build cross-compilation tools.
- **`Makefile`**: Build system for compiling and linking the kernel.

## Getting Started

### Prerequisites

- A cross-compiler targeting `i686-elf`.
- QEMU for running the kernel.
- GRUB for bootloading.

### Build and Run

1. Build the kernel:
     ```bash
     make
     ```

2. Create a disk image:
     ```bash
     make disk/hd.img
     ```

3. Run the kernel in QEMU:
     ```bash
     make run
     ```

4. Debug the kernel:
     ```bash
     make debug
     ```

### Clean Up

To clean the build artifacts:
```bash
make clean
```

To remove the disk image:
```bash
make clean_disk
```

## Testing

Unit tests are implemented in `src/tests.c`. They are executed during kernel initialization.

## License

This project is for educational purposes and does not include a specific license. Use at your own risk.

## Acknowledgments

- Based on resources from [OSDev Wiki](https://wiki.osdev.org/).
- Inspired by Bran's Kernel Development Tutorial.
- Uses GCC and Binutils for cross-compilation.


## Build Instructions

### Prerequisites
- Cross-compiler targeting `i686-elf` (GCC, Binutils)
- QEMU for emulation

### Steps
1. **Set up the cross-compiler**:
   Use the scripts in the `scripts/` directory to build and install the required toolchain:
   ```bash
   ./scripts/install_binutils.sh
   ./scripts/install_gcc.sh
   ./scripts/install_gdb.sh

## Notes

- The kernel is a work in progress and is not intended for production use.