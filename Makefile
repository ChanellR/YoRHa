# Cross compiler and tools
CC=i686-elf-gcc
AS=i686-elf-as
LD=i686-elf-ld
CFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra
LDFLAGS=-T src/linker.ld

# Directories
SRC_DIR=src
BUILD_DIR=build

# Find all source files and convert to object file names in the build directory
SRC_FILES=$(shell find $(SRC_DIR) -type f \( -name '*.c' -o -name '*.s' \))
KERNEL_OBJS=$(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%, $(SRC_FILES:.c=.o))
KERNEL_OBJS := $(KERNEL_OBJS:.s=.o)
KERNEL_BIN=$(BUILD_DIR)/kernel.bin

# QEMU
QEMU=qemu-system-i386

# Targets
all: $(KERNEL_BIN)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(BUILD_DIR)
	$(AS) $< -o $@

$(KERNEL_BIN): $(KERNEL_OBJS)
	$(CC) $(LDFLAGS) -o $(KERNEL_BIN) -ffreestanding -O2 -nostdlib $(KERNEL_OBJS) -lgcc

run: $(KERNEL_BIN)
	$(QEMU) -kernel $(KERNEL_BIN)

clean:
	rm -f $(KERNEL_OBJS) $(KERNEL_ELF) $(KERNEL_BIN)

.PHONY: all clean run
