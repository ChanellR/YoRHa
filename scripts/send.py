import subprocess
import os
import struct

cmd = "qemu-system-i386 -m 1024M -drive file=disk/hd.img,format=raw -kernel build/kernel.bin -serial stdio"

# Start the process
process = subprocess.Popen(
    cmd,  # Launches Python interactive mode as an example
    shell=True,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=False  # This makes the pipes work with strings instead of bytes
    # bufsize=1  # Line-buffered
)

# Prompt the user to select a binary file
binary_file_path = "programs/build/countup.bin"

# Read the binary file content
with open(binary_file_path, "rb") as binary_file:
    binary_data = binary_file.read()    
file_size = struct.pack("<I", len(binary_data))

print(f"Preparing to send {file_size + binary_data}")
input("Press any key to being transmission")
process.stdin.write(file_size + binary_data)
process.stdin.flush()  # Ensure the input is sent
print("Completed")

# pid = int(input("Enter the PID of the process to attach to: "))
# stdin_path = f"/proc/{pid}/fd/0"

# if os.path.exists(stdin_path):
#     with open(stdin_path, 'w') as stdin_fd:
#         stdin_fd.write("Hello from attached stdin!\n")
#         stdin_fd.flush()
# else:
#     print(f"Cannot attach to PID {pid}. Stdin path does not exist.")

# def mem_page(memory_address):
#     table = memory_address >> 22 & 0x3FF
#     page = memory_address >> 12 & 0x3FF
#     return table, page

# MAIN = 0xc02066f0
# ERR_MSG = 0xc0310de0
# NEW_PAGE = 0x401000
# print(mem_page(MAIN))
# print(mem_page(ERR_MSG))
# print(mem_page(NEW_PAGE))
