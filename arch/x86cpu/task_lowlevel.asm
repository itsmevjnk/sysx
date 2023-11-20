section .text

extern task_current
extern vmm_switch
extern x86ext_on

; void task_switch(void* task, void* context)
;  Performs a context switch to the specified task.
;  This is an architecture-specific function and is called in ring 0.
global task_switch
task_switch:
cli ; ensure that interrupt is off, otherwise it will mess up the task switch
cld ; for movsb

mov ebp, [task_current] ; task_current
test ebp, ebp
jz .switch_pd ; skip storing context
mov eax, [ebp + (4 * 11)] ; task_current->type/ready/pid
and eax, 0x00000007 ; extract type
mov esi, [esp + (4 * 2)] ; context
cmp dword [esi], 0x10 ; GS = DS = 0x10 means we're in ring 0 at the time of switching
jne .store_ctx ; we are not interested in the ring 3 case
cmp eax, 1 ; DS = 0x10 and type = 1 means type should've been 2 (ring 3 task running ring 0 code) instead
jne .store_ctx
mov eax, [ebp + (4 * 11)]
and eax, ~0x7
or eax, 2
mov [ebp + (4 * 11)], eax
.store_ctx: ; store context into current task
add esi, (4 * 4) ; skip DS/ES/FS/GS
mov edi, [task_current]
mov ecx, (4 * 3)
rep movsb ; EDI, ESI, EBP
cmp eax, 1 ; were we in ring 3 at the time of the switch? (EAX is from previously)
jne .store_kernel_esp
.store_user_esp:
add esi, 4 ; skip pushed ESP
mov eax, [esi + (4 * 9)] ; user ESP
jmp .store_ctx_cont
.store_kernel_esp:
lodsd ; ESP pushed via PUSHA - this is the value at the point after the vector and exception code are pushed
add eax, (4 * 5) ; to rewrite EIP, CS and EFLAGS
.store_ctx_cont: ; EAX contains user/kernel ESP
stosd
mov ecx, (4 * 4)
rep movsb ; EBX, EDX, ECX, EAX
add esi, (4 * 2) ; skip vector and exception code
movsd ; EIP
add esi, (4 * 1) ; skip CS
movsd ; EFLAGS

mov edi, [task_current]
add edi, 64 ; start of regs_ext

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

.switch_pd: ; switch page directory - since the kernel is mapped across all pages, the stack will remain available
mov ebp, [esp + (4 * 1)] ; task
mov dword [task_current], ebp ; change task_current since we will not be working on it

mov eax, [ebp + (4 * 10)] ; task->vmm
push eax
call vmm_switch
add esp, 4

.load_regs_s0: ; load FPU/MMX/SSE registers
mov edi, ebp
add edi, 64 ; start of regs_ext
test word [x86ext_on], (1 << 3)
jz .no_fxrstor
.fxrstor:
fxrstor [edi]
jmp .load_regs_s1

.no_fxrstor:
test word [x86ext_on], (1 << 0) | (1 << 1)
jz .load_regs_s1 ; there ain't no way a processor can support SSE without supporting FPU or MMX - correct me if I'm wrong...
frstor [edi] ; MMX uses the same regs as FPU so this is enough
test word [x86ext_on], (1 << 2)
jz .load_regs_s1 ; no SSE
ldmxcsr [edi + 108] ; load new MXCSR
movaps xmm0, [edi + 108 + 4 + 0*16]
movaps xmm1, [edi + 108 + 4 + 1*16]
movaps xmm2, [edi + 108 + 4 + 2*16]
movaps xmm3, [edi + 108 + 4 + 3*16]
movaps xmm4, [edi + 108 + 4 + 4*16]
movaps xmm5, [edi + 108 + 4 + 5*16]
movaps xmm6, [edi + 108 + 4 + 6*16]
movaps xmm7, [edi + 108 + 4 + 7*16]

.load_regs_s1: ; load general purpose registers from specified task
mov edi, [ebp + (4 * 0)]
mov esi, [ebp + (4 * 1)]
; EBP will be restored later
; ESP will be restored later
mov ebx, [ebp + (4 * 4)]
mov edx, [ebp + (4 * 5)]
mov ecx, [ebp + (4 * 6)]
; EAX will be restored later

; load ESP and set up stack for IRET
.load_esp:
mov eax, [ebp + (4 * 11)] ; task->type/ready/pid
and eax, 0x00000007 ; extract type
cmp eax, 1
jne .ring0_iret_prep
.ring3_iret_prep:
push 0x23 ; user SS
mov eax, [ebp + (4 * 3)]
push eax ; user ESP
mov eax, [ebp + (4 * 9)]
push eax ; EFLAGS
push 0x1B ; user CS
mov eax, [ebp + (4 * 8)]
push eax ; EIP
jmp .load_regs_s2
.ring0_iret_prep:
mov esp, [ebp + (4 * 3)] ; load ESP
mov eax, [ebp + (4 * 9)]
push eax ; EFLAGS
push 0x08 ; kernel CS
mov eax, [ebp + (4 * 8)]
push eax ; EIP

.load_regs_s2: ; load the rest of the general purpose registers into stack for later popping
mov eax, [ebp + (4 * 7)]
push eax ; EAX
mov eax, [ebp + (4 * 2)]
push eax ; EBP
; ESP is either restored previously (if switching into ring 0)
;     or will be restored by IRET (if switching into ring 3)

.load_dseg: ; load data segments
mov eax, [ebp + (4 * 11)] ; task->type/ready/pid
and eax, 0x00000007 ; extract type
cmp eax, 1
jne .eoi ; skip setting DS/ES/FS/GS for ring 0 since this has been done by the IDT handler
.load_ring3_dseg:
mov ax, 0x23
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax

.eoi: ; send EOI to master PIC (IRQ0) on the PIC handler's behalf (since we'll be skipping over it)
mov al, 0x20
out 0x20, al

.load_regs_s3: ; pop EBP and EAX, and finally EFLAGS and CS:EIP via IRET (plus SS:ESP if switching to ring 3)
pop ebp
pop eax
iretd ; reload EFLAGS and EIP in one go
