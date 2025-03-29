section .text
    org 0x0                 ; Ensure everything starts from 0x0 for a flat binary

    ; === Header ===
    dd 0xDEADBEEF           ; Magic value (4 bytes)
    dd start                ; Entry point (relative offset, 4 bytes)
    dd code_end - start     ; Size of the code segment (4 bytes)

start:
    mov cx, 0               ; Initialize counter

count_loop:
    mov dx, 0x3F8           ; Serial port address
    mov al, '0'             ; Convert counter to ASCII (0-9)
    add al, cl
    out dx, al

    inc cx                  ; Increment counter
    cmp cx, 10              ; Stop after counting to 9
    jl count_loop

    ret

code_end: