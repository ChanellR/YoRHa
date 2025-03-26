#include <interrupts.h>

/* bkerndev - Bran's Kernel Development Tutorial
 *  By:   Brandon F. (friesenb@gmail.com)
 *  Desc: Interrupt Service Routines installer and exceptions
 *
 *  Notes: No warranty expressed or implied. Use at own risk. */

GDTEntry gdt[6];
GDTPtr gp;

IDTEntry idt[256];
IDTPointer idtp;

/* This array is actually an array of function pointers. We use
 *  this to handle custom IRQ handlers for a given IRQ */
void *irq_routines[16] =
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0};


/* This is a simple string array. It contains the message that
 *  corresponds to each and every exception. We get the correct
 *  message by accessing like:
 *  exception_message[interrupt_number] */
char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};


/**
 * Set a GDT descriptor
 *
 * @param num The number for the descriptor to set.
 * @param base Base address
 * @param limit Limit
 * @param access Access permissions
 * @param gran Granularity
 */
void gdt_set_gate(
    int num,
    unsigned long base,
    unsigned long limit,
    unsigned char access,
    unsigned char gran)
{
    /* Base Address */
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    /* Limits */
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0X0F;
    /* Granularity */
    gdt[num].granularity |= (gran & 0xF0);
    /* Access flags */
    gdt[num].access = access;
}

/*
 * gdt_install
 * Install the kernel's GDTs
 */
void gdt_install()
{
    /* GDT pointer and limits */
    gp.limit = (sizeof(GDTEntry) * 5) - 1;
    gp.base = (unsigned int)&gdt;
    /* NULL */
    gdt_set_gate(0, 0, 0, 0, 0);
    /* Code segment */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    /* Data segment */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    /* User code */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    /* User data */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    // write_tss(5, 0x10, 0x0);
    /* Go go go */
    gdt_flush();
    // tss_flush();
}

/*
 * idt_set_gate
 * Set an IDT gate
 */
void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].zero = 0;
    idt[num].flags = flags | 0x60;
}

/*
 * idt_install
 * Install the IDTs
 */
void idt_install() {
    idtp.limit = (sizeof(IDTEntry) * 256) - 1;
    idtp.base = (uintptr_t)&idt;
    memset(&idt, 0, sizeof(IDTEntry) * 256);
    idt_load();
}


void isrs_install()
{
    idt_set_gate(0, (unsigned)isr0, 0x08, 0x8E);
    idt_set_gate(1, (unsigned)isr1, 0x08, 0x8E);
    idt_set_gate(2, (unsigned)isr2, 0x08, 0x8E);
    idt_set_gate(3, (unsigned)isr3, 0x08, 0x8E);
    idt_set_gate(4, (unsigned)isr4, 0x08, 0x8E);
    idt_set_gate(5, (unsigned)isr5, 0x08, 0x8E);
    idt_set_gate(6, (unsigned)isr6, 0x08, 0x8E);
    idt_set_gate(7, (unsigned)isr7, 0x08, 0x8E);

    idt_set_gate(8, (unsigned)isr8, 0x08, 0x8E);
    idt_set_gate(9, (unsigned)isr9, 0x08, 0x8E);
    idt_set_gate(10, (unsigned)isr10, 0x08, 0x8E);
    idt_set_gate(11, (unsigned)isr11, 0x08, 0x8E);
    idt_set_gate(12, (unsigned)isr12, 0x08, 0x8E);
    idt_set_gate(13, (unsigned)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned)isr14, 0x08, 0x8E);
    idt_set_gate(15, (unsigned)isr15, 0x08, 0x8E);

    idt_set_gate(16, (unsigned)isr16, 0x08, 0x8E);
    idt_set_gate(17, (unsigned)isr17, 0x08, 0x8E);
    idt_set_gate(18, (unsigned)isr18, 0x08, 0x8E);
    idt_set_gate(19, (unsigned)isr19, 0x08, 0x8E);
    idt_set_gate(20, (unsigned)isr20, 0x08, 0x8E);
    idt_set_gate(21, (unsigned)isr21, 0x08, 0x8E);
    idt_set_gate(22, (unsigned)isr22, 0x08, 0x8E);
    idt_set_gate(23, (unsigned)isr23, 0x08, 0x8E);

    idt_set_gate(24, (unsigned)isr24, 0x08, 0x8E);
    idt_set_gate(25, (unsigned)isr25, 0x08, 0x8E);
    idt_set_gate(26, (unsigned)isr26, 0x08, 0x8E);
    idt_set_gate(27, (unsigned)isr27, 0x08, 0x8E);
    idt_set_gate(28, (unsigned)isr28, 0x08, 0x8E);
    idt_set_gate(29, (unsigned)isr29, 0x08, 0x8E);
    idt_set_gate(30, (unsigned)isr30, 0x08, 0x8E);
    idt_set_gate(31, (unsigned)isr31, 0x08, 0x8E);
}

void irq_install()
{
    irq_remap(0x20, 0x28);

    idt_set_gate(32, (unsigned)irq0, 0x08, 0x8E);
    idt_set_gate(33, (unsigned)irq1, 0x08, 0x8E);
    idt_set_gate(34, (unsigned)irq2, 0x08, 0x8E);
    idt_set_gate(35, (unsigned)irq3, 0x08, 0x8E);
    idt_set_gate(36, (unsigned)irq4, 0x08, 0x8E);
    idt_set_gate(37, (unsigned)irq5, 0x08, 0x8E);
    idt_set_gate(38, (unsigned)irq6, 0x08, 0x8E);
    idt_set_gate(39, (unsigned)irq7, 0x08, 0x8E);

    idt_set_gate(40, (unsigned)irq8, 0x08, 0x8E);
    idt_set_gate(41, (unsigned)irq9, 0x08, 0x8E);
    idt_set_gate(42, (unsigned)irq10, 0x08, 0x8E);
    idt_set_gate(43, (unsigned)irq11, 0x08, 0x8E);
    idt_set_gate(44, (unsigned)irq12, 0x08, 0x8E);
    idt_set_gate(45, (unsigned)irq13, 0x08, 0x8E);
    idt_set_gate(46, (unsigned)irq14, 0x08, 0x8E);
    idt_set_gate(47, (unsigned)irq15, 0x08, 0x8E);

    idt_set_gate(80, (unsigned)isr80, 0x08, 0x8E);
}

/* All of our Exception handling Interrupt Service Routines will
 *  point to this function. This will tell us what exception has
 *  happened! Right now, we simply halt the system by hitting an
 *  endless loop. All ISRs disable interrupts while they are being
 *  serviced as a 'locking' mechanism to prevent an IRQ from
 *  happening and messing up kernel data structures */
void isr_handler(Registers *r) {
    kprint("Entered\n");
    if (r->int_no < 32)
    {
        kprintf(exception_messages[r->int_no]);
        kprintf(" Exception. System Halted!\n");
        for (;;)
            ;
    }

    if (r->int_no == 80)
    {
        kprintf("System call: %d\n", r->err_code);
    }
}

/* This installs a custom IRQ handler for the given IRQ */
void irq_install_handler(int irq, void (*handler)(Registers *r)) {
    irq_routines[irq] = handler;
}

/* This clears the handler for a given IRQ */
void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

void PIC_sendEOI(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);

    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_disable(void)
{
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */

/*
arguments:
    offset1 - vector offset for master PIC
        vectors on the master become offset1..offset1+7
    offset2 - same for slave PIC: offset2..offset2+7
*/
void irq_remap(int offset1, int offset2)
{
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC1_DATA, offset1); // ICW2: Master PIC vector offset
    outb(PIC2_DATA, offset2); // ICW2: Slave PIC vector offset
    outb(PIC1_DATA, 4);       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC2_DATA, 2);       // ICW3: tell Slave PIC its cascade identity (0000 0010)

    outb(PIC1_DATA, ICW4_8086); // ICW4: have the PICs use 8086 mode (and not 8080 mode)
    outb(PIC2_DATA, ICW4_8086);

    // Unmask both PICs.
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
}


/* Each of the IRQ ISRs point to this function, rather than
 *  the 'fault_handler' in 'isrs.c'. The IRQ Controllers need
 *  to be told when you are done servicing them, so you need
 *  to send them an "End of Interrupt" command (0x20). There
 *  are two 8259 chips: The first exists at 0x20, the second
 *  exists at 0xA0. If the second controller (an IRQ from 8 to
 *  15) gets an interrupt, you need to acknowledge the
 *  interrupt at BOTH controllers, otherwise, you only send
 *  an EOI command to the first controller. If you don't send
 *  an EOI, you won't raise any more IRQs */
void irq_handler(Registers *r)
{
    /* This is a blank function pointer */
    void (*handler)(Registers *r);

    /* Find out if we have a custom handler to run for this
     *  IRQ, and then finally, run it */
    handler = irq_routines[r->int_no - 32];
    if (handler)
    {
        handler(r);
    }

    /* If the IDT entry that was invoked was greater than 40
     *  (meaning IRQ8 - 15), then we need to send an EOI to
     *  the slave controller */
    if (r->int_no >= 40)
    {
        outb(0xA0, 0x20);
    }

    /* In either case, we need to send an EOI to the master
     *  interrupt controller too */
    outb(0x20, 0x20);
}