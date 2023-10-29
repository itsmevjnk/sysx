#ifndef EXEC_ELF_MACHINES_H
#define EXEC_ELF_MACHINES_H

/* e_machine values */
#define EM_NONE         0x0000 // no machine
#define EM_M32          0x0001 // AT&T WE 32100
#define EM_SPARC        0x0002 // SPARC
#define EM_386          0x0003 // Intel i386 (x86)
#define EM_68K          0x0004 // Motorola 68k
#define EM_88K          0x0005 // Motorola 88k
#define EM_INTEL_MCU    0x0006 // Intel MCUs
#define EM_860          0x0007 // Intel i860
#define EM_MIPS         0x0008 // MIPS RS3000 Big-Endian
#define EM_S370         0x0009 // IBM System/370
#define EM_MIPS_RS4_BE  0x000A // MIPS RS4000 Big-Endian
#define EM_MIPS_RS3_LE  0x000A // MIPS RS3000 Little-Endian
#define EM_PARISC       0x000F // HP PA-RISC
#define EM_960          0x0013 // Intel i960
#define EM_PPC          0x0014 // PowerPC
#define EM_PPC64        0x0015 // PowerPC (64-bit)
#define EM_S390         0x0016 // IBM S390/S390x00
#define EM_SPU_SPC      0x0017 // IBM SPU/SPC
#define EM_V800         0x0024 // NEC V800
#define EM_FR20         0x0025 // Fujitsu FR20
#define EM_RH32         0x0026 // TRW RH-32
#define EM_RCE          0x0027 // Motorola RCE
#define EM_ARM          0x0028 // ARM (AArch32)
#define EM_ALPHA        0x0029 // DEC Alpha
#define EM_SUPERH       0x002A // SuperH
#define EM_SPARC_V9     0x002B // SPARC Version 9
#define EM_TRICORE      0x002C // Siemens TriCore
#define EM_ARGONAUT     0x002D // Argonaut RISC
#define EM_H8_300       0x002E // Hitachi H8/300
#define EM_H8_300H      0x002F // Hitachi H8/300H
#define EM_H8S          0x0030 // Hitachi H8S
#define EM_H8_500       0x0031 // Hitachi H8/500
#define EM_IA64         0x0032 // Intel Itanium (IA-64)
#define EM_MIPSX        0x0033 // Stanford MIPS-X
#define EM_COLDFIRE     0x0034 // Motorola ColdFire
#define EM_68HC12       0x0035 // Motorola 68HC12
#define EM_MMA          0x0036 // Fujitsu MMA
#define EM_PCP          0x0037 // Siemens PCP
#define EM_NCPU         0x0038 // Sony nCPU (Cell?)
#define EM_NDR1         0x0039 // Denso NDR1
#define EM_STARCORE     0x003A // Motorola Star*Core
#define EM_ME16         0x003B // Toyota ME16
#define EM_ST100        0x003C // STMicroelectronics ST100
#define EM_TINYJ        0x003D // Advanced Logic Corp. TinyJ
#define EM_AMD64        0x003E // AND x86-64
#define EM_SONY_DSP     0x003F // Sony DSP
#define EM_PDP10        0x0040 // DEC PDP-10
#define EM_PDP11        0x0041 // DEC PDP-11
#define EM_FX66         0x0042 // Siemens FX66
#define EM_ST9P         0x0043 // STMicroelectronics ST9+
#define EM_ST7          0x0044 // STMicroelectronics ST7
#define EM_68HC16       0x0045 // Motorola 68HC16
#define EM_68HC11       0x0046 // Motorola 68HC11
#define EM_68HC08       0x0047 // Motorola 68HC08
#define EM_68HC05       0x0048 // Motorola 68HC05
#define EM_SVX          0x0049 // Silicon Graphics SVx
#define EM_ST19         0x004A // STMicroelectronics ST19
#define EM_VAX          0x004B // DEC VAX
#define EM_AXIS32       0x004C // Axis Communications 32-bit embedded processor
#define EM_INFINEON32   0x004D // Infineon 32-bit embedded processor
#define EM_E14_DSP64    0x004E // Element14 64-bit DSP
#define EM_LSI_DSP16    0x004F // LSI 16-bit DSP
#define EM_TMS320C6K    0x008C // TMS320C6000 family
#define EM_E2K          0x00AF // Elbrus e2k
#define EM_ARM64        0x00B7 // ARM AArch64
#define EM_Z80          0x00DC // Zilog Z80
#define EM_RISCV        0x00F3 // RISC-V
#define EM_BPF          0x00F7 // Berkeley Packet Filter
#define EM_65C816       0x0101 // WDC 65C816

#endif