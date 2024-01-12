extern task_yield_noirq

global mutex_acquire
mutex_acquire:
push ebp
mov ebp, esp
xor ecx, ecx
inc ecx ; ECX = 1
mov edx, [ebp + 4 * 2] ; m (also ptr to m->locked)
.wait:
xor eax, eax
cmpxchg [edx], ecx ; if [EDX] == EAX=0: set ZF and store ECX=1 into [EDX]; otherwise, clear ZF and load [EDX] into EAX (which is discarded by xor eax, eax)
jz .done
call task_yield_noirq
jmp .wait
.done:
leave
ret

global mutex_release
mutex_release:
push ebp
mov ebp, esp
mov eax, [ebp + 4 * 2]
mov dword [eax], 0
; call task_yield_noirq
leave
ret
