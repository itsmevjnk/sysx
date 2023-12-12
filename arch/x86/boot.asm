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
.top:
resb 65536 
.bottom:
kernelpt_start: resd 1 ; start of kernel page table (physical addr)

global x86ext_on
x86ext_on: resw 1 ; FPU = bit 0, MMX = bit 1, SSE = bit 2, XSAVE = bit 3

; for converting between physical and virtual addresses (higher half only)
%define PHYS(x)					(x - 0xC0000000)
%define VIRT(x)					(x + 0xC0000000)

%define PD(x)                   (x - 0xC0000000)
%define PT(x)                   (x - 0xC0000000 + 4096)

; kernel entry point
section .pre.text
global _start

extern __kernel_start
extern __kernel_end

extern __text_start
extern __text_end

extern __rodata_start
extern __rodata_end

extern __data_start
extern __data_end

extern __bss_start
extern __bss_end

extern vmm_default_pd
extern vmm_krnlpt

extern __rmap_start
extern __rmap_end

extern vmm_current
extern vmm_kernel

; EAX = 0x2badb002, EBX = ptr to Multiboot info structure (to be relocated ASAP)
_start:
; set up stack (we will be using the kernel stack, which will be re-used in higher half)
mov esp, PHYS(stack.bottom)

; disable the PICs now (so IRQs don't cause interrupts later and give us weird bugs)
push eax
mov al, 0x20
out 0xa0, al
out 0x20, al
mov al, 0xff
out 0xa1, al
out 0x21, al
cli
pop eax

cld ; clear direction flag
mov edi, PHYS(__kernel_end) ; load EDI with the kernel end address
; from now on, EDI MUST NOT be touched

cmp eax, 0x2badb002
jnz _vmm ; if we're not booting with a Multiboot-compliant bootloader, skip Multiboot relocation

; Multiboot relocation code - START
_mb_relocate:

; first of all, relocate the info structure
mov esi, ebx
mov ebx, edi ; where the info structure will be located
; we've loaded EDI earlier
mov ecx, 116 ; number of bytes to be copied
rep movsb ; Ivy Bridge+ moment

mov edx, [ebx] ; get flags

; check cmdline
test edx, (1 << 2)
jz cmdline.end
mov esi, [ebx + 16]
add edi, 0xC0000000
mov [ebx + 16], edi
sub edi, 0xC0000000
cmdline:
lodsb ; by doing this we also get the copying character (to check if we've reached the end)
stosb
test al, al
jnz cmdline
.end:

; check modules
test edx, (1 << 3)
jz mods.end
mov ecx, [ebx + 20] ; mods_count
test ecx, ecx
jz mods.end ; nothing to relocate
; relocate structures
push edx ; will be touched by MUL
mov eax, 16
mul ecx
pop edx
mov ecx, eax ; number of bytes to be copied (16 * mods_count)
mov esi, [ebx + 24] ; mods_addr
add edi, 0xC0000000
mov [ebx + 24], edi ; new mods_addr
sub edi, 0xC0000000
rep movsb
; relocate modules
mov ecx, [ebx + 20] ; mods_count
mov ebp, [ebx + 24] ; mods_addr
sub ebp, 0xC0000000
mods:
push ecx ; we don't want to lose modules count
mov esi, [ebp] ; mod_start
mov ecx, [ebp + 4] ; mod_end
sub ecx, esi ; module size
add edi, 0xC0000000
mov [ebp], edi ; new mod_start
add ecx, edi
mov [ebp + 4], ecx ; new mod_end
sub ecx, edi
sub edi, 0xC0000000
rep movsb
mov esi, [ebp + 8] ; string ptr
add edi, 0xC0000000
mov [ebp + 8], edi ; new string ptr
sub edi, 0xC0000000
.strloop:
lodsb
stosb
test al, al
jnz .strloop
pop ecx
loop mods
.end:

; relocate memory map
test edx, (1 << 6)
jz mmap.end
mov ecx, [ebx + 44] ; mmap_length
test ecx, ecx
jz mmap.end
mmap:
mov esi, [ebx + 48] ; mmap_addr
add edi, 0xC0000000
mov [ebx + 48], edi ; new mmap_addr
sub edi, 0xC0000000
rep movsb
.end:

add ebx, 0xC0000000 ; convert Multiboot pointer to virtual

; Multiboot relocation code - END

; PMM initialization code - START

extern pmm_bitmap
extern pmm_frames
_pmm:
add edi, 0xC0000000
mov dword [PHYS(pmm_bitmap)], edi ; PMM bitmap pointer
sub edi, 0xC0000000
; now we have to figure out how much memory do we want to allocate for PMM bitmap
; (that is, how many frames there are)
xor edx, edx ; EDX: number of frames
mov esi, ebx
sub esi, (0xC0000000 - 44)
mov ecx, [esi] ; mmap_length
add esi, 4
mov esi, [esi] ; mmap_addr
sub esi, 0xC0000000
add ecx, esi ; maximum address
.iterate:
mov eax, [esi + 4] ; low base
add eax, [esi + 12] ; low length
jc .max ; maximum amount of frames
shr eax, 12
cmp edx, eax
jae .next ; EDX >= EAX
mov edx, eax
.next:
add esi, [esi] ; size
add esi, 4
cmp esi, ecx
jb .iterate
.done:
mov [PHYS(pmm_frames)], edx
; wipe bitmap
xor eax, eax
not eax ; this will set EAX to 0xFFFFFFFF (which means all frames will be initially marked as unavailable)
mov ecx, edx
shr ecx, 5 ; equivalent to ECX / 32
rep stosd
jmp _vmm
.max:
mov edx, 1048576
jmp .done

; PMM initialization code - END

; VMM initialization code - START

_vmm:
; 4K align EDI
mov eax, edi
and eax, 0xfff
and edi, 0xfffff000
test eax, eax
jz .edi_aligned
add edi, 0x1000
.edi_aligned:
mov [PHYS(kernelpt_start)], edi ; for later comparison (since we don't need to map the page tables)
mov ebp, PD(vmm_default_pd) ; ptr to page directory
; wipe page directory in the case that it haven't been wiped yet
push edi
xor eax, eax
mov edi, ebp
mov ecx, 1024 * 4
rep stosb ; more speed?
; set up recursive mapping
mov eax, ebp ; ptr to page directory
or eax, (1 << 0) | (1 << 1) ; present, writable, supervisor
mov edi, __rmap_start ; start of recursive mapping region (4M aligned)
shr edi, 22 ; page directory entry idx
shl edi, 2 ; times 4
add edi, ebp ; EDI now contains pointer to the PDE
stosd ; store EAX content to EDI - now the PD is recursively mapped
pop edi
mov esi, 0x100000 ; ESI = physical address of the page we're mapping
.map: ; mapping code begins here
; check for page table
mov ebp, esi
shr ebp, 22 ; page directory entry
shl ebp, 2 ; times 4
add ebp, PD(vmm_default_pd)
mov eax, [ebp]
and eax, ~0xFFF ; discard flags
test eax, eax ; EAX == 0?
jnz .pt_created
; no page table for current page, let's make it then
mov eax, edi
or eax, (1 << 0) | (1 << 1) ; present, writable, supervisor
mov [ebp], eax ; identity map
add ebp, 768 * 4
mov [ebp], eax ; higher half map
; we don't need to store the page tables' virtual addresses, now that they are recursively mapped
push edi
; wipe page table
xor eax, eax
mov ecx, 1024 * 4
rep stosb ; this will also increment EDI for us
pop eax
.pt_created: ; this code assumes the page table pointer is stored in EAX
; do the mapping in page table
mov ebp, esi
shr ebp, 12 ; page table entry
shl ebp, 2
and ebp, 0xfff ; remove the page directory entry junk
add ebp, eax ; ptr to page table entry
mov eax, esi
or eax, (1 << 0) | (1 << 1) | (1 << 8) ; present, writable, supervisor, global
mov [ebp], eax
add esi, 0x1000 ; next page
cmp esi, [PHYS(kernelpt_start)]
jb .map ; if we haven't mapped everything, go back and map more
; we're ready to go, so let's enable paging
mov eax, PD(vmm_default_pd)
mov cr3, eax
mov eax, cr4
or eax, (1 << 7) | (1 << 4) ; enable global paging and PSE
mov cr4, eax
mov eax, cr0
or eax, 0x80010000 ; enable paging as well as write protect enforcement
mov cr0, eax

mov eax, _hstart ; this MUST be in higher half, or things will break
jmp eax ; hasta la vista!

; VMM initialization code - END

; mm/addr.c
extern kernel_start
extern kernel_end
extern text_start
extern text_end
extern rodata_start
extern rodata_end
extern data_start
extern data_end
extern bss_start
extern bss_end
extern kernel_stack_top
extern kernel_stack_bottom

extern mb_info

extern kinit

extern srand

; higher half kernel entry point
section .text
_hstart:
mov esp, stack.bottom ; dump stack, and use higher half stack now
mov dword [kernel_stack_bottom], stack.bottom ; save stack bottom address
mov dword [kernel_stack_top], stack.top ; save stack top address

mov [mb_info], ebx
add edi, 0xC0000000 ; bring kernel_end to higher half too
mov [kernel_end], edi
mov eax, __rmap_start
and eax, 0xF0000000
mov [kernel_start], eax
mov dword [text_start], __text_start
mov dword [text_end], __text_end
mov dword [rodata_start], __rodata_start
mov dword [rodata_end], __rodata_end
mov dword [data_start], __data_start
mov dword [data_end], __data_end
mov dword [bss_start], __bss_start
mov dword [bss_end], __bss_end

; wipe identity mapping portion off page directory
xor eax, eax
mov edi, vmm_default_pd
mov ecx, __rmap_start ; start of recursive mapping region (4M aligned)
shr ecx, 22 ; page directory entry idx
shl ecx, 2 ; times 4
rep stosb
mov eax, cr3
mov cr3, eax ; quick and dirty TLB invalidation

mov dword [vmm_current], PD(vmm_default_pd)
mov dword [vmm_kernel], PD(vmm_default_pd)

.fpu: ; initialize the FPU and SSE (if possible)
mov word [x86ext_on], 0x55AA ; use x86ext_on for temporary storage too

mov eax, cr0
and al, ~((1 << 2) | (1 << 3)) ; disable CR0.TS and CR0.EM - force FPU access
or al, (1 << 5) | (1 << 1) ; enable CR0.NE and CR0.MP
mov cr0, eax

fninit
fnstsw [x86ext_on]
cmp word [x86ext_on], 0
mov word [x86ext_on], 0
jne .mmx ; no FPU
or word [x86ext_on], (1 << 0)

.mmx:
xor ecx, ecx ; subfunction 0
mov eax, 1 ; function 1
cpuid

bt edx, 23
jnc .sse
or word [x86ext_on], (1 << 1)

.sse:
bt edx, 25
jnc .xsave ; no SSE
or word [x86ext_on], (1 << 2)
mov eax, cr4
or ax, (1 << 9) | (1 << 10) ; set CR4.OSFXSR and CR4.OSXMMEXCPT - enable SSE
mov cr4, eax

.xsave:
bt ecx, 26
jnc .prng ; no XSAVE
or word [x86ext_on], (1 << 3)

.prng: ; seed the PRNG using RDSEED and/or RDRAND if possible
; we'll use the CPUID results from the previous code
bt ecx, 30 ; ECX bit 30 indicates RDRAND support
jnc .no_rdrand
mov ecx, 100 ; try 100 times
.attempt_rdrand:
rdrand eax
jc .rdrand_done
loop .attempt_rdrand
jmp .no_rdrand ; fail
.rdrand_done:
push eax
call srand
pop eax ; discard

.no_rdrand:
xor ecx, ecx
mov eax, 7
cpuid
bt ebx, 18 ; EBX bit 18 indicates RDSEED support
jnc .prng_done
mov ecx, 100
.attempt_rdseed:
rdseed eax
jc .prng_done
loop .attempt_rdseed
jmp .prng_done
push eax
call srand
pop eax

.prng_done:
xor ebp, ebp ; initialize call frame

call kinit

.halt:
hlt ; this is good enough and is also more energy-efficient
jmp .halt
