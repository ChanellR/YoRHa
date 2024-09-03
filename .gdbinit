# Attach to QEMU gdb instance
file build/kernel.bin
target remote localhost:1234

# Defaults
set print pretty on

# Breakpoints
break main

# Run until breakpoint
continue