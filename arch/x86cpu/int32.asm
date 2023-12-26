bits 32

%define INT32_BASE                      0x7C00
%define RELOC(x)                        (((x) - int32.reloc_start) + INT32_BASE)
%define INT32_REGS_END                  INT32_BASE
%define INT32_REGS_START                (INT32_REGS_END - 13 * 2)

section .text
; void int32(uint8_t vector, int32_regs_t* regs)
;  Calls the specified 16-bit (real mode) interrupt vector.
global int32
extern idt_desc
extern lapic_base
extern apic_enabled
int32: use32
push ebp ; set up stack frame here, which helps with debugging
mov ebp, esp

; preserve all registers in case we ever end up using them
pushf
pusha

cli ; disable interrupts right away - in case things happen
cld ; also do this to make sure string directions are correct

; save PIC masks
in al, 0xA1 ; slave PIC mask
mov ah, al ; AH = slave, AL = master
in al, 0x21 ; master PIC mask
push eax

; reset PIC vectors
mov ax, 0x7008 ; AH = slave, AL = master
push .copy
.reset_pic:
push ax
mov al, 0x11 ; ICW1_INIT | ICW1_ICW4
out 0x20, al
out 0xA0, al
pop ax
out 0x21, al ; master PIC vector
mov al, ah
out 0xA1, al ; slave PIC vector
mov al, 0x04 ; slave to IRQ2
out 0x21, al
shr al, 1 ; 0x02
out 0xA1, al
shr al, 1 ; 0x01
out 0x21, al
out 0xA1, al
ret

.copy: ; copy registers struct to before our code
mov esi, [ebp + 3 * 4] ; regs
mov edi, INT32_REGS_START
mov ecx, (INT32_REGS_END - INT32_REGS_START)
rep movsb

; relocate our interrupt-calling code to 0x7C00
mov esi, .reloc_start
mov edi, RELOC(.reloc_start)
mov ecx, (.reloc_end - .reloc_start)
rep movsb

; fill in the gaps
mov eax, [ebp + 2 * 4] ; vector
mov byte [RELOC(.vect)], al
mov dword [RELOC(.esp_orig)], esp ; ESP (for restoring upon completion)
mov dword [RELOC(.ebp_orig)], ebp ; EBP (for restoring upon completion)

mov esp, INT32_REGS_START ; set ESP to the start of our registers struct (so we can pop everything out)
jmp word 0x28:RELOC(.reloc_start) ; jump to our main code (now in INT32_BASE) - we also set the CS to the 16-bit segment here

.reloc_start: use16 ; 16-bit protected mode
mov ax, 0x30 ; set 16-bit data segment
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

; disable paging and protected mode
mov eax, cr0
and eax, ~((1 << 0) | (1 << 31))
mov cr0, eax

jmp word 0x0000:RELOC(.rmode)

.rmode: use16 ; 16-bit real mode
xor ax, ax ; set SS to 0 (as we would in real mode), so we can pop the registers out
mov ss, ax

lidt [RELOC(.idt16_ptr)] ; set IDTR for 16-bit interrupts

; pop GS, FS, ES, DS (in that order)
pop gs
pop fs
pop es
pop ds

popa ; pop general purpose regs
popf ; pop flags register

sti ; enable interrupts (in case our interrupt needs it)
db 0xCD ; INT imm8
.vect: db 0 ; interrupt vector number goes here
cli ; disable interrupts again

pushf ; push flags register
pusha ; push general purpose regs

; push DS, ES, FS, GS
push ds
push es
push fs
push gs

; restore ESP and EBP
db 0x66, 0xBC ; mov esp, imm32
.esp_orig: dd 0
db 0x66, 0xBD ; mov ebp, imm32
.ebp_orig: dd 0

; re-enable paging and protected mode
mov eax, cr0
or eax, (1 << 0) | (1 << 31)
mov cr0, eax

jmp dword 0x08:.reloc_end

.idt16_ptr:
dw 0x03FF
dd 0x00000000

.reloc_end: use32 ; back out of relocated code
; set DS, ES, FS, GS and SS to 32-bit data segment
mov ax, 0x10
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

lidt [idt_desc] ; set IDTR back to original

; copy resulting registers back to the struct
mov esi, INT32_REGS_START
mov edi, [ebp + 3 * 4] ; regs
mov ecx, (INT32_REGS_END - INT32_REGS_START)
rep movsb

; issue EOI to both PICs
mov al, 0x20
out 0x20, al
out 0xA0, al

; reset PIC vectors to what SysX uses
mov ax, 0x2820
call .reset_pic

; restore PIC masks
pop eax
out 0x21, al
mov al, ah
out 0xA1, al

; check if APIC is enabled, and if so, send EOI to APIC
mov eax, [apic_enabled]
test eax, eax
jz .done ; nothing to do here

mov eax, [lapic_base]
; mov dword [eax + 0x0F0], 0xFF | (1 << 8)
mov dword [eax + 0x0B0], 0 ; EOI

.done:
; restore all registers
popa
popf

; finally restore EBP and exit
leave
ret
