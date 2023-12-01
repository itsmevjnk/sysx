%ifdef ARCH_MEMCPY
; void* memcpy(void* dest, const void* src, size_t n)
;  Copies n characters from src to dest and returns dest.
global memcpy
memcpy:
push ebp
mov ebp, esp
push edi
push esi
pushf
mov edi, [ebp + 2 * 4] ; dest
mov esi, [ebp + 3 * 4] ; src
mov ecx, [ebp + 4 * 4] ; n
cld ; set to auto-incrementing
rep movsb
popf
pop esi
pop edi
leave
ret
%endif

%ifdef ARCH_MEMMOVE
; void* memmove(void* dest, const void* src, size_t n)
;  Copies n characters from src to dest and returns dest.
global memmove
memmove:
push ebp
mov ebp, esp
push edi
push esi
pushf
mov edi, [ebp + 2 * 4] ; dest
mov esi, [ebp + 3 * 4] ; src
mov ecx, [ebp + 4 * 4] ; n
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
pop esi
pop edi
leave
ret
%endif

%ifdef ARCH_MEMSET
; void* memset(void* str, int c, size_t n)
;  Copies the character c to the first n characters of the
;  string str, then returns str.
global memset
memset:
push ebp
mov ebp, esp
push edi
push esi
pushf
mov edi, [ebp + 2 * 4] ; str
mov eax, [ebp + 3 * 4] ; c (but we'll only use the lowest 8 bits)
mov ecx, [ebp + 4 * 4] ; n
cld ; set to auto-incrementing
rep stosb
popf
pop esi
pop edi
leave
ret
%endif

%ifdef ARCH_MEMSET16
; void* memset16(void* str, uint16_t c, size_t n)
;  Copies the character c to the first n characters of the
;  string str, then returns str.
global memset16
memset16:
push ebp
mov ebp, esp
push edi
push esi
pushf
mov edi, [ebp + 2 * 4] ; str
mov eax, [ebp + 3 * 4] ; c (but we'll only use the lowest 16 bits)
mov ecx, [ebp + 4 * 4] ; n
cld ; set to auto-incrementing
rep stosw
popf
pop esi
pop edi
leave
ret
%endif

%ifdef ARCH_MEMSET32
; void* memset32(void* str, uint32_t c, size_t n)
;  Copies the character c to the first n characters of the
;  string str, then returns str.
global memset32
memset32:
push ebp
mov ebp, esp
push edi
push esi
pushf
mov edi, [ebp + 2 * 4] ; str
mov eax, [ebp + 3 * 4] ; c
mov ecx, [ebp + 4 * 4] ; n
cld ; set to auto-incrementing
rep stosd
popf
pop esi
pop edi
leave
ret
%endif
