; void usermode_switch(uintptr_t iptr, uintptr_t sptr)
;  Switch to user mode, returning to the caller (if iptr = 0) or
;  to a specified non-zero instruction pointer, and set the stack
;  pointer to sptr (if sptr != 0).
global usermode_switch
usermode_switch:
pushf ; save flags
cli ; disable interrupts
pop ecx ; temporarily store original flags in ECX (which is also a scratch register)

mov edx, [esp + 4] ; sptr
cmp edx, 0
jnz .iptr
mov edx, esp ; use current ESP
add edx, 12 ; skip past args and return address

.iptr:
mov eax, [esp + 8] ; iptr
cmp eax, 0
jnz .load
mov eax, [esp] ; return address

.load:
push eax ; save return address for later

mov ax, 0x23 ; ring 3 data segment + RPL
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

pop eax ; this will be pushed again later

add esp, 12 ; discard current return address and iptr + sptr

push 0x23 ; SS
push edx ; user mode ESP
push ecx ; EFLAGS
push 0x1B ; CS: ring 3 code segment + RPL
push eax ; EIP
iretd ; goodbye!

; void task_yield_noirq()
;  Yields to the next task on demand (i.e. without IRQs).
extern task_kernel
global task_yield_noirq
task_yield_noirq:
push ebp
mov ebp, esp
cmp dword [task_kernel], 0
jz .done ; do not yield if tasking is not available
int 0x9F ; INT 0x9F will be configured to call task_yield()
.done:
leave
ret