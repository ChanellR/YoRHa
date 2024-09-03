# Commands that build the os kernel

Follow this [tutorial](https://wiki.osdev.org/GCC_Cross-Compiler) to get source folders for the required packages.

### assemble bootstrap
`i686-elf-as src/boot.s -o build/boot.o`

### compile kernel
`i686-elf-gcc -c src/kernel.c -o build/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra`

### build kernel
`i686-elf-gcc -T linker.ld -o build/myos.bin -ffreestanding -O2 -nostdlib src/boot.o src/kernel.o -lgcc`

### build iso
```
mkdir -p isodir/boot/grub
cp build/myos.bin isodir/boot/myos.bin
cp grub.cfg isodir/boot/grub/grub.cfg
grub-mkrescue -o build/myos.iso isodir
```

## Run QEMU

### .iso
`qemu-system-i386 -cdrom build/myos.iso`

### .bin with multiboot header
`qemu-system-i386 -kernel build/myos.bin`

## Read Disk Image
`dd if=disk/hd.img of=/dev/stdout bs=512 count=<sector_count> skip=<start>`