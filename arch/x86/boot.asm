; Multiboot header
%define ALIGN       (1 << 0)
%define MEMINFO     (1 << 1)
%define FLAGS       (ALIGN | MEMINFO)
%define MAGIC       0x1BADB002
%define CHECKSUM    -(MAGIC + FLAGS)

section .multiboot
align 4
dd MAGIC
dd FLAGS
dd CHECKSUM

; stack
section .bss
align 16
stack:
.bottom:
resb 4096 ; 4K of stack, probably enough to get us started
.top:

; kernel entry point
section .text
global _start
_start:
mov esp, stack.top ; set esp to stack top

; print Hello, World! (TODO: move this to C)
mov esi, str
mov edi, 0xb8000
mov ah, 0x2f ; white on green
cld ; clear direction flag
.next:
lodsb
cmp al, 0
jz .done
stosw
jmp .next
.done:

; halt since we have nothing else to do
cli
.end:
hlt
jmp .end

; data that we will be printing
section .data
str: db "Hello, World!", 0
