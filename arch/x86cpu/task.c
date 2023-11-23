#include <arch/x86cpu/task.h>
#include <kernel/log.h>
#include <arch/x86cpu/gdt.h>
#include <hal/intr.h>
#include <mm/vmm.h>
#include <stdlib.h>
#include <string.h>

static size_t task_size = sizeof(task_t); // size of each task structure (including extended registers)

extern uint16_t x86ext_on;

void* task_create_stub() {
    task_t* task = kmalloc_ext(task_size, 16, NULL);
    if(task == NULL) {
        kerror("cannot allocate memory for new task");
        return NULL;
    }

    memset(task, 0, task_size); // new task

    task->regs.eflags |= (1 << 9); // enable interrupts in all cases
    // set MXCSR; not sure if this is needed but we'll do it anyway
    if(x86ext_on & (1 << 3)) task->regs_ext[6] = 0x1F80;
    else if(x86ext_on & (1 << 2)) task->regs_ext[27] = 0x1F80; 

    return task;
}

void task_delete_stub(void* task) {
    kfree(task);
}

uintptr_t task_get_iptr(void* task) {
    return ((task_t*)task)->regs.eip;
}

void task_set_iptr(void* task, uintptr_t iptr) {    
    ((task_t*)task)->regs.eip = iptr;
}

uintptr_t task_get_sptr(void* task) {
    return ((task_t*)task)->regs.esp;
}

void task_set_sptr(void* task, uintptr_t sptr) {
    ((task_t*)task)->regs.esp = sptr;
}

task_common_t* task_common(void* task) {
    return &((task_t*)task)->common;
}

void task_do_yield_noirq(uint8_t vector, void* context) {
    (void) vector;
    task_yield(context);
}

void task_init() {
    kdebug("task structure size without regs_ext: %u", task_size);
    if(x86ext_on & (1 << 3)) {
        /* FXSAVE is supported - regs_ext is a 512-byte buffer for the instruction */
        task_size += 512;
    } else {
        /* FXSAVE is not supported - store XMM registers manually */
        if(x86ext_on & ((1 << 0) | (1 << 1))) {
            /* FPU/MMX is available - so is FSAVE */
            task_size += 108; // FSAVE takes 94 or 108 bytes depending on mode
        }
        if(x86ext_on & (1 << 2)) {
            /* SSE is available */
            task_size += 4 + 128; // 4 bytes for MXCSR + 8x 128-bit registers (XMM0-7) - task_size is assumed to be 16-byte aligned
        }
    }
    kdebug("task structure size with regs_ext: %u", task_size);
    intr_handle(0x8F, (void*) &task_do_yield_noirq);
    task_init_stub();
}
