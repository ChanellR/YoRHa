# Cross compiler and tools
CC=i686-elf-gcc
AS=i686-elf-as
LD=i686-elf-ld
CFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude -g 
LDFLAGS=-T linker.ld -g

# Directories
SRC_DIR=src
BUILD_DIR=build
INCLUDE_DIR=include 

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
	$(AS) $< -g -o $@

$(KERNEL_BIN): $(KERNEL_OBJS)
	$(CC) $(LDFLAGS) -o $(KERNEL_BIN) -ffreestanding -O2 -nostdlib $(KERNEL_OBJS) -lgcc

disk/hd.img: 
	qemu-img create -f raw disk/hd.img 1M
	
run: $(KERNEL_BIN) disk/hd.img
	$(QEMU) -m 1024M -drive file=disk/hd.img,format=raw -kernel $(KERNEL_BIN)

debug: $(KERNEL_BIN) disk/hd.img
	$(QEMU) -s -S -m 1024M -drive file=disk/hd.img,format=raw -kernel $(KERNEL_BIN) -display gtk,zoom-to-fit=on -d int

clean_all: clean clean_disk

clean:
	@rm -rf $(BUILD_DIR) 

clean_disk:
	@rm -f disk/hd.img

.PHONY: all clean run clean_disk clean_all
