#include <arch/x86cpu/task.h>
#include <kernel/log.h>
#include <arch/x86cpu/gdt.h>
#include <hal/intr.h>
#include <mm/vmm.h>
#include <stdlib.h>
#include <string.h>

void* task_create_stub(void* src_task) {
    task_t* task = kmalloc(sizeof(task_t));
    if(task == NULL) {
        kerror("cannot allocate memory for new task");
        return NULL;
    }

    if(src_task == NULL) memset(task, 0, sizeof(task_t)); // new task
    else memcpy(task, src_task, sizeof(task_t)); // copy from source

    task->regs.eflags |= (1 << 9); // enable interrupts in all cases

    return task;
}

void task_delete_stub(void* task) {
    kfree(task);
}

void* task_get_vmm(void* task) {
    return ((task_t*)task)->common.vmm;
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
    intr_handle(0x8F, (void*) &task_do_yield_noirq);
    task_init_stub();
}
