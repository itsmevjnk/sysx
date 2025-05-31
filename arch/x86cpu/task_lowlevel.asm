section .text

extern task_current
extern x86ext_on
extern proc_pidtab
extern tss_entry
extern vmm_current
extern timer_tick

extern apic_enabled:weak
extern lapic_base:weak

; void task_switch(void* task, void* context)
;  Performs a context switch to the specified task.
;  This is an architecture-specific function and is called in ring 0.
global task_switch
task_switch:
cli ; ensure that interrupt is off, otherwise it will mess up the task switch
cld ; for movsb

mov edi, [task_current] ; task_current
test edi, edi
jz .switch_pd ; skip storing context if task_current is null

.save_general: ; save general registers from context
mov esi, [esp + (4 * 2)] ; context
add esi, (4 * 4) ; skip DS/ES/FS/GS (which is to be derived from CS)
; EDI set to task_current->regs
mov ecx, (4 * 8) ; EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX (in this order)
rep movsb
add esi, (4 * 2) ; skip vector and exc_code
mov ecx, (4 * 3) ; EIP, CS, EFLAGS (in this order)
rep movsb
mov eax, [edi - (4 * 2)] ; read CS into EAX (to check ring)
test eax, 0b11 ; either one of these set is enough for us (since we don't use rings 1 or 2)
jz .no_save_usr ; interrupt occurred from ring 0 - skip saving user SS and ESP (which aren't valid anyway)
.save_usr:
mov ecx, (4 * 2) ; user ESP, user SS (in this order)
rep movsb
jmp .save_ext
.no_save_usr:
add edi, (4 * 2) ; not saving anything, so we skip

.save_ext: ; save extended (FPU/MMX/SSE) regs
add edi, 24 ; skip task_current->common
test word [x86ext_on], (1 << 3)
jz .no_fxsave
.fxsave:
fxsave [edi]
jmp .switch_pd

.no_fxsave:
test word [x86ext_on], (1 << 0) | (1 << 1)
jz .switch_pd ; there ain't no way a processor can support SSE without supporting FPU or MMX - correct me if I'm wrong...
fsave [edi] ; MMX uses the same regs as FPU so this is enough
test word [x86ext_on], (1 << 2)
jz .switch_pd ; no SSE
stmxcsr [edi + 108] ; store MXCSR
movaps [edi + 108 + 4 + 0*16], xmm0
movaps [edi + 108 + 4 + 1*16], xmm1
movaps [edi + 108 + 4 + 2*16], xmm2
movaps [edi + 108 + 4 + 3*16], xmm3
movaps [edi + 108 + 4 + 4*16], xmm4
movaps [edi + 108 + 4 + 5*16], xmm5
movaps [edi + 108 + 4 + 6*16], xmm6
movaps [edi + 108 + 4 + 7*16], xmm7

.switch_pd: ; switch page directory
mov ebp, [esp + (4 * 1)] ; task
mov dword [task_current], ebp ; change task_current since we will not be working on it

mov eax, [ebp + (4 * 8 + 4 * 5)] ; task->type/ready/pid
or eax, (1 << 3) ; set ready flag (as we're switching into it, so it has to be ready)
mov [ebp + (4 * 8 + 4 * 5)], eax
shr eax, 4 ; discard type and ready
shl eax, 2 ; multiply by 4
add eax, dword [proc_pidtab] ; address into proc_pidtab
mov eax, [eax] ; proc
mov eax, [eax + 2 * 4] ; proc->vmm - TODO: do we need mutex_acquire and mutex_release here?
cmp eax, dword [vmm_current]
je .load_esp0
mov cr3, eax
mov [vmm_current], eax

.load_esp0: ; load ring 0 ESP
mov eax, [ebp + 4 * (8 + 5 + 1)] ; task->stack_bottom
mov dword [tss_entry + (4 * 1)], eax ; tss_entry.esp0

.load_ext: ; load FPU/MMX/SSE registers
mov esi, ebp ; task
add esi, (4 * 8 + 4 * 5 + 24) ; start of regs_ext
test word [x86ext_on], (1 << 3)
jz .no_fxrstor
.fxrstor:
fxrstor [esi]
jmp .load_general

.no_fxrstor:
test word [x86ext_on], (1 << 0) | (1 << 1)
jz .load_general ; there ain't no way a processor can support SSE without supporting FPU or MMX - correct me if I'm wrong...
frstor [esi] ; MMX uses the same regs as FPU so this is enough
test word [x86ext_on], (1 << 2)
jz .load_general ; no SSE
ldmxcsr [esi + 108] ; load new MXCSR
movaps xmm0, [esi + 108 + 4 + 0*16]
movaps xmm1, [esi + 108 + 4 + 1*16]
movaps xmm2, [esi + 108 + 4 + 2*16]
movaps xmm3, [esi + 108 + 4 + 3*16]
movaps xmm4, [esi + 108 + 4 + 4*16]
movaps xmm5, [esi + 108 + 4 + 5*16]
movaps xmm6, [esi + 108 + 4 + 6*16]
movaps xmm7, [esi + 108 + 4 + 7*16]

.load_general: ; load general purpose registers
mov esi, ebp ; task
mov eax, [esi + (4 * 9)] ; read CS
add eax, 0x08 ; turn it into DS
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

; figure out where to put stack in
test eax, 0b11 ; test for ring 3 again
jz .load_general_ring0
.load_general_ring3: ; ring 3 - use ring 0 reentry stack
mov edi, [esp + (4 * 2)] ; reuse context to save on stack - after all, we're not going back to idt_handler_stub
mov ecx, (4 * 8 + 4 * 5) ; task->regs is definitely smaller than context, so we can copy everything in
jmp .load_general_copy ; by the end of this we need to have the stack address pushed on the stack (so it can be popped out in .jump)
.load_general_ring0: ; ring 0 - append to destination task's stack
mov edi, [ebp + (4 * 3)] ; read ESP (which should point at interrupt vector number - idt_context_t offset 48)
sub edi, (4 * (8 + 3)) - (4 * (2 + 3)) ; ESP + 4 * 2 (EIP) + 4 * 3 (stack top before interrupt) - 4 * 3 (for iret) - 4 * 8 (for popa)
mov ecx, (4 * 8 + 4 * 3) ; do not put user stuff in
.load_general_copy:
mov esp, edi ; point ESP to where we'll do the dump
rep movsb

.eoi: ; send EOI to all PICs on the PIC handler's behalf (since we'll be skipping over it)
mov eax, apic_enabled
test eax, eax ; test if apic_enabled is NULL (i.e. no APIC support)
jz .pic_eoi ; no APIC support
mov eax, [apic_enabled]
test eax, eax ; test if apic_enabled is true (i.e. APIC is up)
jz .pic_eoi ; APIC is supported but is not enabled
.apic_eoi:
mov eax, [lapic_base]
add eax, 0x0B0 ; LAPIC EOI register
mov dword [eax], 0
jmp .jump
.pic_eoi:
mov al, 0x20
out 0xA0, al
out 0x20, al

; reset RTC
mov al, 0x0C
out 0x70, al
in al, 0x71

.jump:
popa
iret ; and we're out :)

; void* task_fork(struct proc* proc)
;  Forks the current task and returns the child task.
global task_fork
extern task_fork_stub
; NOTE: The i386 System V ABI specifies that:
;  - EAX, ECX and EDX are scratch registers.
;  - Functions must preserve EBX, ESI, EDI, EBP and ESP.
;  - A stack frame is created by pushing EBP, then setting EBP to ESP (thus EBP stores the ptr to its original value in the stack).
task_fork:
push ebp ; create a new stack frame - this helps with debugging, and makes it easier to unwind the instruction and stack pointers later on.
mov ebp, esp

xor eax, [ebp + 4 * 2] ; proc
push eax
call task_fork_stub ; get new task - new task struct's ptr will be in EAX
add esp, 4
test eax, eax ; test EAX = NULL
jz .done ; cannot create task - exit here.

.save_ctx: ; save context
pushf
cli ; for safety
cld ; for string instructions - although DF should have been cleared already
pusha

; store PUSHA-saved registers
mov esi, esp
mov edi, eax ; task
mov ecx, 8 * 4 ; EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX (we'll fix EBP and ESP later)
rep movsb

; store EBP
mov eax, [ebp] ; the EBP that we pushed previously
; fix EBP (EBP + new stack bottom - old stack bottom)
mov edx, [task_current]
mov edx, [edx + (8 + 5) * 4 + 1 * 4] ; task_current->stack_bottom
sub edx, [edi + 5 * 4 + 1 * 4] ; task->stack_bottom
neg edx ; EDX = new stack bottom - old stack bottom
add eax, edx
mov [edi - 6 * 4], eax

; store ESP
mov eax, ebp
add eax, 2 * 4 ; discard EBP and return address (i.e. manually doing leave & ret)
; fix ESP (ESP + new stack bottom - old stack bottom)
add eax, edx ; we'll use the value we calculated in EDX
mov [edi - 5 * 4], eax

; store EIP
mov eax, [ebp + 1 * 4] ; return address pushed onto the stack by the caller
stosd

; store CS
xor eax, eax
mov ax, cs
stosd

; store EFLAGS
movsd ; ESI points at EFLAGS in the stack (pushed with PUSHF), and EDI points at task->regs.eflags (incremented by STOSD)

; no need to store user SS or ESP at this point since we're returning into ring 0 on the child task

.save_ext: ; store FPU/MMX and SSE registers
add edi, (4 * 2) + 24 ; start of regs_ext
test word [x86ext_on], (1 << 3)
jz .no_fxsave
.fxsave: ; use FXSAVE to save FPU/MMX and SSE registers in one go
fxsave [edi]
jmp .done
.no_fxsave:
test word [x86ext_on], (1 << 0) | (1 << 1)
jz .done ; no FPU/MMX support - nothing to be stored
fsave [edi] ; MMX uses the same regs as FPU so this is enough
test word [x86ext_on], (1 << 2)
jz .done ; no SSE
stmxcsr [edi + 108] ; store MXCSR
movaps [edi + 108 + 4 + 0*16], xmm0
movaps [edi + 108 + 4 + 1*16], xmm1
movaps [edi + 108 + 4 + 2*16], xmm2
movaps [edi + 108 + 4 + 3*16], xmm3
movaps [edi + 108 + 4 + 4*16], xmm4
movaps [edi + 108 + 4 + 5*16], xmm5
movaps [edi + 108 + 4 + 6*16], xmm6
movaps [edi + 108 + 4 + 7*16], xmm7

popa
or dword [eax + 13 * 4], (1 << 3) ; task->ready = 1 - do this before re-enabling interrupts
popf

.done:
leave ; short for mov esp, ebp & pop ebp
ret
