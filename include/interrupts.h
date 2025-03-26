#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stddef.h>
#include <stdint.h>
#include <asm/cpu_io.h>
#include <string.h>
#include <vga.h>

#define PIC_EOI 0x20 /* End-of-interrupt command code */

#define PIC1 0x20 /* IO base address for master PIC */
#define PIC2 0xA0 /* IO base address for slave PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

#define ICW1_ICW4 0x01      /* Indicates that ICW4 will be present */
#define ICW1_SINGLE 0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* Level triggered (edge) mode */
#define ICW1_INIT 0x10      /* Initialization - required! */

#define ICW4_8086 0x01       /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* Special fully nested (not) */

inline void enable_interrupts() {
	asm volatile ("sti");
}

inline void disable_interrupts() {
	asm volatile ("cli");
}

typedef struct __attribute__((packed)) {
    // In this order
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    // not as sure about useresp, and ss,
    // haven't tried going from ring 3 -> 0.
    uint32_t eip, cs, eflags, useresp, ss;
}Registers;


typedef struct __attribute__((packed))
{
    /* Limits */
    unsigned short limit_low;
    /* Segment address */
    unsigned short base_low;
    unsigned char base_middle;
    /* Access modes */
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
}GDTEntry;

/*
 * GDT pointer
 */
typedef struct __attribute__((packed))
{
    unsigned short limit;
    unsigned int base;
}GDTPtr;

/*
 * IDT Entry
 */
typedef struct __attribute__((packed))
{
    unsigned short base_low;
    unsigned short sel;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
}IDTEntry;

/*
 * IDT pointer
 */
typedef struct __attribute__((packed))
{
    unsigned short limit;
    uintptr_t base;
}IDTPointer;

void gdt_install();
void idt_install();
void isrs_install();
void irq_install();
void irq_install_handler(int irq, void (*handler)(Registers *r));
void irq_uninstall_handler(int irq);
void irq_remap(int offset1, int offset2);

/**
 * (ASM) gdt_flush
 * Reloads the segment registers
 */
extern void gdt_flush();
extern void idt_load();

/* These are function prototypes for all of the exception
 *  handlers: The first 32 entries in the IDT are reserved
 *  by Intel, and are designed to service exceptions! */
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

/* These are own ISRs that point to our special IRQ handler
 *  instead of the regular 'fault_handler' function */
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void isr80();

#endif // INTERRUPTS_H