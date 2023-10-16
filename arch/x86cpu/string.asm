%ifdef ARCH_MEMCPY
; void* memcpy(void* dest, const void* src, size_t n)
;  Copies n characters from src to dest and returns dest.
global memcpy
memcpy:
pusha
pushf
mov edi, [esp + 40] ; dest
mov esi, [esp + 44] ; src
mov ecx, [esp + 48] ; n
cld ; set to auto-incrementing
rep movsb
popf
popa
ret
%endif

%ifdef ARCH_MEMMOVE
; void* memmove(void* dest, const void* src, size_t n)
;  Copies n characters from src to dest and returns dest.
global memmove
memmove:
pusha
pushf
mov edi, [esp + 40] ; dest
mov esi, [esp + 44] ; src
mov ecx, [esp + 48] ; n
cld ; set to auto-incrementing
cmp edi, esi
je .done ; dest = src - nothing to be done
jb .move
; dest > src - reverse direction
add edi, ecx
dec edi
add esi, ecx
dec esi
std
.move:
rep movsb
.done:
popf
popa
ret
%endif

%ifdef ARCH_MEMSET
; void* memset(void* str, int c, size_t n)
;  Copies the character c to the first n characters of the
;  string str, then returns str.
global memset
memset:
pusha
pushf
mov edi, [esp + 40] ; str
mov eax, [esp + 44] ; c (but we'll only use the lowest 8 bits)
mov ecx, [esp + 48] ; n
cld ; set to auto-incrementing
rep stosb
popf
popa
ret
%endif