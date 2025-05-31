#ifndef ARCH_X86CPU_TASK_H
#define ARCH_X86CPU_TASK_H

#include <exec/task.h>

/* task description structure */
typedef struct {
    struct context {
        uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
        uint32_t eip, cs, eflags, esp_usr, ss_usr;
    } __attribute__((packed)) regs; // virtually copiable from idt_context_t
    task_common_t common;
    uint32_t regs_ext[]; // extended registers
} __attribute__((packed)) task_t;

#endif