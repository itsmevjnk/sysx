#include <exec/syscall.h>
#include <hal/intr.h>
#include <arch/x86cpu/idt.h>

void syscall_handler(uint8_t vector, idt_context_t* context) {
    (void) vector;
    
    switch(context->eax) {
        default:
            syscall_handler_stub(&context->eax, &context->ecx, &context->edx, &context->ebx, &context->esi, &context->edi);
            break;
    }
}

void syscall_init() {
    intr_handle(0x80, (void*) &syscall_handler);
}
