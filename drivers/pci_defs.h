#ifndef DRIVERS_PCI_DEFS_H
#define DRIVERS_PCI_DEFS_H

#include <stddef.h>
#include <stdint.h>

/* PCI configuration space offset */
// common fields
#define PCI_CFG_VID                     0x00 // Vendor ID (word)
#define PCI_CFG_PID                     0x02 // Product ID (word)
#define PCI_CFG_COMMAND                 0x04 // Command (word)
#define PCI_CFG_STATUS                  0x06 // Status (word)
#define PCI_CFG_REVID                   0x08 // Revision ID (byte)
#define PCI_CFG_PROG_IF                 0x09 // Prog IF (byte)
#define PCI_CFG_SUBCLASS                0x0A // Subclass (byte)
#define PCI_CFG_CLASS                   0x0B // Class (byte)
#define PCI_CFG_CACHE_SZ                0x0C // Cache line size (byte)
#define PCI_CFG_LAT_TIMER               0x0D // Latency timer (byte)
#define PCI_CFG_HDR_TYPE                0x0E // Header type (byte)
#define PCI_CFG_BIST                    0x0F // BIST (byte)

// header type 0x0
#define PCI_HDRTYPE_DEVICE              0x00
#define PCI_CFG_H0_BAR0                 0x10 // BAR0 (dword)
#define PCI_CFG_H0_BAR1                 0x14 // BAR1 (dword)
#define PCI_CFG_H0_BAR2                 0x18 // BAR2 (dword)
#define PCI_CFG_H0_BAR3                 0x1C // BAR3 (dword)
#define PCI_CFG_H0_BAR4                 0x20 // BAR4 (dword)
#define PCI_CFG_H0_BAR5                 0x24 // BAR5 (dword)
#define PCI_CFG_H0_CISPTR               0x28 // CardBus CIS pointer (dword)
#define PCI_CFG_H0_SUBSYS_VID           0x2C // Subsystem vendor ID (word)
#define PCI_CFG_H0_SUBSYS_ID            0x2E // Subsystem ID (word)
#define PCI_CFG_H0_EXPROM_BASE          0x30 // Expansion ROM base address (dword)
#define PCI_CFG_H0_CAPABILITIES         0x34 // Capabilities (byte)
#define PCI_CFG_H0_IRQ_LINE             0x3C // Interrupt line (byte)
#define PCI_CFG_H0_IRQ_PIN              0x3D // Interrupt pin (byte)
#define PCI_CFG_H0_MIN_GRANT            0x3E // Minimum burst period (byte)
#define PCI_CFG_H0_MAX_LAT              0x3F // Maximum bus access time (byte)

// header type 0x1 (PCI-to-PCI bridge)
#define PCI_HDRTYPE_BRIDGE_PCI          0x01
#define PCI_CFG_H1_BAR0                 0x10 // BAR0 (dword)
#define PCI_CFG_H1_BAR1                 0x14 // BAR1 (dword)
#define PCI_CFG_H1_PRI_BUS              0x18 // Primary bus no. (byte)
#define PCI_CFG_H1_SEC_BUS              0x19 // Secondary bus no. (byte)
#define PCI_CFG_H1_SUB_BUS              0x1A // Subordinate bus no. (byte)
#define PCI_CFG_H1_SEC_LAT_TIMER        0x1B // Secondary latency timer (byte)
#define PCI_CFG_H1_IO_BASE_LO           0x1C // I/O base lower 8 bits (byte)
#define PCI_CFG_H1_IO_LIMIT_LO          0x1D // I/O limit lower 8 bits (byte)
#define PCI_CFG_H1_SEC_STATUS           0x1E // Secondary Status (word)
#define PCI_CFG_H1_MEM_BASE             0x20 // Memory base (word)
#define PCI_CFG_H1_MEM_LIMIT            0x22 // Memory limit (word)
#define PCI_CFG_H1_PMEM_BASE_LO         0x24 // Prefetchable memory base lower 16 bits (word)
#define PCI_CFG_H1_PMEM_LIMIT_LO        0x26 // Prefetchable memory limit lower 16 bits (word)
#define PCI_CFG_H1_PMEM_BASE_HI         0x28 // Prefetchable memory base upper 32 bits (dword)
#define PCI_CFG_H1_PMEM_LIMIT_HI        0x2C // Prefetchable memory limit upper 32 bits (dword)
#define PCI_CFG_H1_IO_BASE_HI           0x30 // I/O base upper 16 bits (word)
#define PCI_CFG_H1_IO_LIMIT_HI          0x32 // I/O limit upper 16 bits (word)
#define PCI_CFG_H1_CAPABILITIES         0x34 // Capabilities (byte)
#define PCI_CFG_H1_EXPROM_BASE          0x38 // Expansion ROM base address (dword)
#define PCI_CFG_H1_IRQ_LINE             0x3C // Interrupt line (byte)
#define PCI_CFG_H1_IRQ_PIN              0x3D // Interrupt pin (byte)
#define PCI_CFG_H1_BRIDGE_CTL           0x3E // Bridge control (word)

// header type 0x2 (PCI-to-CardBus bridge)
#define PCI_HDRTYPE_BRIDGE_CARDBUS      0x02
#define PCI_CFG_H2_CARDBUS_BASE         0x10 // CardBus socket/ExCa base address (dword)
#define PCI_CFG_H2_CAPABILITIES_OFF     0x14 // Offset of capabilities list (byte)
#define PCI_CFG_H2_SEC_STATUS           0x16 // Secondary status (word)
#define PCI_CFG_H2_PCI_BUS              0x18 // PCI bus no. (byte)
#define PCI_CFG_H2_CARDBUS_BUS          0x19 // CardBus bus no. (byte)
#define PCI_CFG_H2_SUB_BUS              0x1A // Subordinate bus no. (byte)
#define PCI_CFG_H2_CARDBUS_LAT_TIMER    0x1B // CardBus latency timer (byte)
#define PCI_CFG_H2_MEM0_BASE            0x1C // Memory base address 0 (dword)
#define PCI_CFG_H2_MEM0_LIMIT           0x20 // Memory limit 0 (dword)
#define PCI_CFG_H2_MEM1_BASE            0x24 // Memory base address 1 (dword)
#define PCI_CFG_H2_MEM1_LIMIT           0x28 // Memory limit 1 (dword)
#define PCI_CFG_H2_IO0_BASE             0x2C // I/O base address 0 (dword)
#define PCI_CFG_H2_IO0_LIMIT            0x30 // I/O limit 0 (dword)
#define PCI_CFG_H2_IO1_BASE             0x34 // I/O base address 0 (dword)
#define PCI_CFG_H2_IO1_LIMIT            0x38 // I/O limit 0 (dword)
#define PCI_CFG_H2_IRQ_LINE             0x3C // Interrupt line (byte)
#define PCI_CFG_H2_IRQ_PIN              0x3D // Interrupt pin (byte)
#define PCI_CFG_H2_BRIDGE_CTL           0x3E // Bridge control (word)
#define PCI_CFG_H2_SUBSYS_PID           0x40 // Subsystem product ID (word)
#define PCI_CFG_H2_SUBSYS_VID           0x42 // Subsystem vendor ID (word)
#define PCI_CFG_H2_LEGACY_BASE          0x44 // 16-bit PC Card legacy mode base address (dword)

#define PCI_HDRTYPE_MF_MASK             (1 << 7) // multifunction device mask

/* PCI class, subclass and Prog IF codes */
#define PCI_CLASS_UNCLASSIFIED                  0x00 // Class 00: Unclassified
#define PCI_UCL_NONVGA                          0x00 // Subclass 00:00 - Non-VGA-compatible Unclassified Device
#define PCI_UCL_VGA                             0x01 // Subclass 00:01 - VGA-compatible Unclassified Device

#define PCI_CLASS_STOR_CTRLR                    0x01 // Class 01: Mass Storage Controller
#define PCI_MSC_SCSI_CTRLR                      0x00 // Subclass 01:00 - SCSI Bus Controller
#define PCI_MSC_IDE_CTRLR                       0x01 // Subclass 01:01 - IDE Controller
#define PCI_MSC_IDE_MASK_ISA                    0x00 // Subclass 01:01 Prog IF mask - ISA controller
#define PCI_MSC_IDE_MASK_PCI                    0x05 // Subclass 01:01 Prog IF mask - PCI controller
#define PCI_MSC_IDE_MASK_SWITCH                 0x0A // Subclass 01:01 Prog IF mask - supports switching to ISA/PCI mode
#define PCI_MSC_IDE_MASK_BM                     0x80 // Subclass 01:01 Prog IF mask - supports bus mastering
#define PCI_MSC_FLOPPY_CTRLR                    0x02 // Subclass 01:02 - Floppy Disk Controller
#define PCI_MSC_IPI_CTRLR                       0x03 // Subclass 01:03 - IPI Bus Controller
#define PCI_MSC_RAID_CTRLR                      0x04 // Subclass 01:04 - RAID Controller
#define PCI_MSC_ATA_CTRLR                       0x05 // Subclass 01:05 - ATA Controller
#define PCI_MSC_ATA_DMA_SINGLE                  0x20 // Subclass 01:05 Prog IF 20 - ATA controller with single DMA
#define PCI_MSC_ATA_DMA_CHAINED                 0x30 // Subclass 01:05 Prog IF 30 - ATA controller with chained DMA
#define PCI_MSC_SATA_CTRLR                      0x06 // Subclass 01:06 - Serial ATA (SATA) Controller
#define PCI_MSC_SATA_VENDOR                     0x00 // Subclass 01:06 Prog IF 00 - vendor specific interface
#define PCI_MSC_SATA_AHCI                       0x01 // Subclass 01:06 Prog IF 01 - AHCI 1.0
#define PCI_MSC_SATA_SSB                        0x02 // Subclass 01:06 Prog IF 02 - Serial Storage Bus
#define PCI_MSC_SAS_CTRLR                       0x07 // Subclass 01:07 - Serial Attached SCSI (SAS) Controller
#define PCI_MSC_SAS                             0x00 // Subclass 01:07 Prog IF 00 - SAS
#define PCI_MSC_SAS_SSB                         0x01 // Subclass 01:07 Prog IF 01 - Serial Storage Bus
#define PCI_MSC_NVM_CTRLR                       0x08 // Subclass 01:08 - Non-volatile Memory Controller
#define PCI_MSC_NVM_NVMHCI                      0x01 // Subclass 01:08 Prog IF 01 - NVMHCI controller
#define PCI_MSC_NVM_NVME                        0x02 // Subclass 01:08 Prog IF 02 - NVM Express (NVMe) controller
#define PCI_MSC_OTHER                           0x80 // Subclass 01:80 - Other Mass Storage Controller

#define PCI_CLASS_NET_CTRLR                     0x02 // Class 02: Network Controller
#define PCI_NET_ETH_CTRLR                       0x00 // Subclass 02:00 - Ethernet Controller
#define PCI_NET_TOKRING_CTRLR                   0x01 // Subclass 02:01 - Token Ring Controller
#define PCI_NET_FDDI_CTRLR                      0x02 // Subclass 02:02 - FDDI Controller
#define PCI_NET_ATM_CTRLR                       0x03 // Subclass 02:03 - ATM Controller
#define PCI_NET_ISDN_CTRLR                      0x04 // Subclass 02:04 - ISDN Controller
#define PCI_NET_WFIP_CTRLR                      0x05 // Subclass 02:05 - WorldFip Controller
#define PCI_NET_PICMG_CTRLR                     0x06 // Subclass 02:06 - PICMG 2.14 Multi Computing Controller
#define PCI_NET_IFBAND_CTRLR                    0x07 // Subclass 02:07 - Infiniband Controller
#define PCI_NET_FABRIC_CTRLR                    0x08 // Subclass 02:08 - Fabric Controller
#define PCI_NET_OTHER                           0x80 // Subclass 02:80 - Other Network Controller

#define PCI_CLASS_DISP_CTRLR                    0x03 // Class 03: Display Controller
#define PCI_DSP_VGA_CTRLR                       0x00 // Subclass 03:00 - VGA Compatible Controller
#define PCI_DSP_VGA                             0x00 // Subclass 03:00 Prog IF 00 - VGA Controller
#define PCI_DSP_VGA_8514                        0x01 // Subclass 03:00 Prog IF 01 - 8514-compatible Controller
#define PCI_DSP_XGA_CTRLR                       0x01 // Subclass 03:01 - XGA Controller
#define PCI_DSP_3D_CTRLR                        0x02 // Subclass 03:02 - 3D Controller
#define PCI_DSP_OTHER                           0x80 // Subclass 03:80 - Other Display Controller

#define PCI_CLASS_MM_CTRLR                      0x04 // Class 04: Multimedia Controller
#define PCI_MED_VIDEO_CTRLR                     0x00 // Subclass 04:00 - Multimedia Video Controller
#define PCI_MED_AUDIO_CTRLR                     0x01 // Subclass 04:01 - Multimedia Audio Controller
#define PCI_MED_TEL_DEV                         0x02 // Subclass 04:02 - Computer Telephony Device
#define PCI_MED_AUDIO_DEV                       0x03 // Subclass 04:03 - Audio Device
#define PCI_MED_OTHER                           0x80 // Subclass 04:80 - Other Multimedia Controller

#define PCI_CLASS_MEM_CTRLR                     0x05 // Class 05: Memory Controller
#define PCI_MEM_RAM_CTRLR                       0x00 // Subclass 05:00 - RAM Controller
#define PCI_MEM_FLASH_CTRLR                     0x01 // Subclass 05:01 - Flash Controller
#define PCI_MEM_OTHER                           0x80 // Subclass 05:80 - Other Memory Controller

#define PCI_CLASS_BRIDGE                        0x06 // Class 06: Bridge
#define PCI_BRG_HOST_BRIDGE                     0x00 // Subclass 06:00 - Host Bridge
#define PCI_BRG_ISA_BRIDGE                      0x01 // Subclass 06:01 - ISA Bridge
#define PCI_BRG_EISA_BRIDGE                     0x02 // Subclass 06:02 - EISA Bridge
#define PCI_BRG_MCA_BRIDGE                      0x03 // Subclass 06:03 - MCA Bridge
#define PCI_BRG_PCI_BRIDGE                      0x04 // Subclass 06:04 - PCI-to-PCI Bridge
#define PCI_BRG_PCI_NORMDEC                     0x00 // Subclass 06:04 Prog IF 00 - normal decode
#define PCI_BRG_PCI_SUBDEC                      0x01 // Subclass 06:04 Prog IF 01 - subtractive decode
#define PCI_BRG_PCMCIA_BRIDGE                   0x05 // Subclass 06:05 - PCMCIA Bridge
#define PCI_BRG_NUBUS_BRIDGE                    0x06 // Subclass 06:06 - NuBus Bridge
#define PCI_BRG_CARDBUS_BRIDGE                  0x07 // Subclass 06:07 - CardBus Bridge
#define PCI_BRG_RACEWAY_BRIDGE                  0x08 // Subclass 06:08 - RACEway Bridge
#define PCI_BRG_RWY_TRANSPARENT                 0x00 // Subclass 06:08 Prog IF 00 - transparent mode
#define PCI_BRG_RWY_ENDPOINT                    0x01 // Subclass 06:08 Prog IF 01 - endpoint mode
#define PCI_BRG_PCI_BRIDGE_ALT                  0x09 // Subclass 06:09 - PCI-to-PCI Bridge
#define PCI_BRG_PCIALT_ST_PRI                   0x40 // Subclass 06:09 Prog IF 40 - semi-transparent, primary bus towards host CPU
#define PCI_BRG_PCIALT_ST_SEC                   0x80 // Subclass 06:09 Prog IF 80 - semi-transparent, secondary bus towards host CPU
#define PCI_BRG_IFBAND_BRIDGE                   0x0A // Subclass 06:0A - Infiniband-to-PCI Host Bridge
#define PCI_BRG_OTHER                           0x80 // Subclass 06:80 - Other Bridge

#define PCI_CLASS_COMM_CTRLR                    0x07 // Class 07: Simple Communication Controller
#define PCI_COM_SER_CTRLR                       0x00 // Subclass 07:00 - Serial Controller
#define PCI_COM_SER_8250                        0x00 // Subclass 07:00 Prog IF 00 - 8250-compatible
#define PCI_COM_SER_16450                       0x01 // Subclass 07:00 Prog IF 01 - 16450-compatible
#define PCI_COM_SER_16550                       0x02 // Subclass 07:00 Prog IF 02 - 16550-compatible
#define PCI_COM_SER_16650                       0x03 // Subclass 07:00 Prog IF 03 - 16650-compatible
#define PCI_COM_SER_16750                       0x04 // Subclass 07:00 Prog IF 04 - 16750-compatible
#define PCI_COM_SER_16850                       0x05 // Subclass 07:00 Prog IF 05 - 16850-compatible
#define PCI_COM_SER_16950                       0x06 // Subclass 07:00 Prog IF 06 - 16950-compatible
#define PCI_COM_PAR_CTRLR                       0x01 // Subclass 07:01 - Parallel Controller
#define PCI_COM_PAR_SPP                         0x00 // Subclass 07:01 Prog IF 00 - Standard Parallel Port
#define PCI_COM_PAR_BPP                         0x01 // Subclass 07:01 Prog IF 01 - Bi-directional Parallel Port
#define PCI_COM_PAR_ECP                         0x02 // Subclass 07:01 Prog IF 02 - ECP 1.x-compliant Parallel Port
#define PCI_COM_PAR_1284_CTRLR                  0x03 // Subclass 07:01 Prog IF 03 - IEEE 1284 Controller
#define PCI_COM_PAR_1284_TGT                    0xFE // Subclass 07:01 Prog IF FE - IEEE 1284 Target Device
#define PCI_COM_MPSER_CTRLR                     0x02 // Subclass 07:02 - Multiport Serial Controller
#define PCI_COM_MODEM                           0x03 // Subclass 07:03 - Modem
#define PCI_COM_MOD_GENERIC                     0x00 // Subclass 07:03 Prog IF 00 - Generic Modem
#define PCI_COM_MOD_HAYES_16450                 0x01 // Subclass 07:03 Prog IF 01 - Hayes 16450-compatible Interface
#define PCI_COM_MOD_HAYES_16550                 0x01 // Subclass 07:03 Prog IF 02 - Hayes 16550-compatible Interface
#define PCI_COM_MOD_HAYES_16650                 0x01 // Subclass 07:03 Prog IF 03 - Hayes 16650-compatible Interface
#define PCI_COM_MOD_HAYES_16750                 0x01 // Subclass 07:03 Prog IF 04 - Hayes 16750-compatible Interface
#define PCI_COM_GPIB_CTRLR                      0x04 // Subclass 07:04 - IEEE 488.1/2 (GPIB) Controller
#define PCI_COM_SMCARD_CTRLR                    0x05 // Subclass 07:05 - Smart Card Controller
#define PCI_COM_OTHER                           0x80 // Subclass 07:80 - Other Simple Communication Controller

#define PCI_CLASS_BASE_SYS_PERIPH               0x08 // Class 08: Base System Peripheral
#define PCI_BSP_PIC                             0x00 // Subclass 08:00 - PIC
#define PCI_BSP_PIC_8259                        0x00 // Subclass 08:00 Prog IF 00 - Generic 8259-compatible PIC
#define PCI_BSP_PIC_ISA                         0x01 // Subclass 08:00 Prog IF 01 - ISA-compatible PIC
#define PCI_BSP_PIC_EISA                        0x02 // Subclass 08:00 Prog IF 02 - EISA-compatible PIC
#define PCI_BSP_PIC_IOAPIC                      0x10 // Subclass 08:00 Prog IF 10 - I/O APIC Interrupt Controller
#define PCI_BSP_PIC_IOXAPIC                     0x20 // Subclass 08:00 Prog IF 20 - I/O(x) APIC Interrupt Controller
#define PCI_BSP_DMA                             0x01 // Subclass 08:01 - DMA Controller
#define PCI_BSP_DMA_8237                        0x00 // Subclass 08:01 Prog IF 00 - Generic 8259-compatible DMA Controller
#define PCI_BSP_DMA_ISA                         0x01 // Subclass 08:01 Prog IF 01 - ISA-compatible DMA Controller
#define PCI_BSP_DMA_EISA                        0x02 // Subclass 08:01 Prog IF 02 - EISA-compatible DMA Controller
#define PCI_BSP_TIMER                           0x02 // Subclass 08:02 - Timer
#define PCI_BSP_TMR_8254                        0x00 // Subclass 08:02 Prog IF 00 - Generic 8254-compatible Timer
#define PCI_BSP_TMR_ISA                         0x01 // Subclass 08:02 Prog IF 01 - ISA-compatible Timer
#define PCI_BSP_TMR_EISA                        0x02 // Subclass 08:02 Prog IF 02 - EISA-compatible Timer
#define PCI_BSP_TMR_HPET                        0x03 // Subclass 08:02 Prog IF 03 - HPET
#define PCI_BSP_RTC                             0x03 // Subclass 08:03 - RTC
#define PCI_BSP_RTC_GENERIC                     0x00 // Subclass 08:03 Prog IF 00 - Generic RTC
#define PCI_BSP_RTC_ISA                         0x01 // Subclass 08:03 Prog IF 01 - ISA-compatible RTC
#define PCI_BSP_PCI_HOTPLUG_CTRLR               0x04 // Subclass 08:04 - PCI Hot-plug Controller
#define PCI_BSP_SD_CTRLR                        0x05 // Subclass 08:05 - SD Host Controller
#define PCI_BSP_IOMMU                           0x06 // Subclass 08:06 - IOMMU Controller
#define PCI_BSP_OTHER                           0x80 // Subclass 08:80 - Other Base System Peripheral

#define PCI_CLASS_INPUT_CTRLR                   0x09 // Class 09: Input Device Controller
#define PCI_INP_KBD_CTRLR                       0x00 // Subclass 09:00 - Keyboard Controller
#define PCI_INP_PEN                             0x01 // Subclass 09:01 - Digitizer Pen
#define PCI_INP_MOUSE_CTRLR                     0x02 // Subclass 09:02 - Mouse Controller
#define PCI_INP_SCAN_CTRLR                      0x03 // Subclass 09:03 - Scanner Controller
#define PCI_INP_GAME_CTRLR                      0x04 // Subclass 09:04 - Game Port Controller
#define PCI_INP_GME_GENERIC                     0x00 // Subclass 09:04 Prog IF 00 - Generic Game Port Controller
#define PCI_INP_GME_EXTENDED                    0x10 // Subclass 09:04 Prog IF 10 - Extended Game Port Controller
#define PCI_INP_OTHER                           0x80 // Subclass 09:80 - Other Input Device Controller

#define PCI_CLASS_DOCK                          0x0A // Class 0A: Docking Station
#define PCI_DOK_GENERIC                         0x00 // Subclass 0A:00 - Generic Docking Station
#define PCI_DOK_OTHER                           0x80 // Subclass 0A:80 - Other Docking Station

#define PCI_CLASS_CPU                           0x0B // Class 0B: Processor
#define PCI_CPU_386                             0x00 // Subclass 0B:00 - Intel 386
#define PCI_CPU_486                             0x01 // Subclass 0B:01 - Intel 486
#define PCI_CPU_PENTIUM                         0x02 // Subclass 0B:02 - Intel Pentium
#define PCI_CPU_PENTIUM_PRO                     0x03 // Subclass 0B:03 - Intel Pentium Pro
#define PCI_CPU_ALPHA                           0x10 // Subclass 0B:10 - DEC Alpha
#define PCI_CPU_PPC                             0x20 // Subclass 0B:20 - PowerPC
#define PCI_CPU_MIPS                            0x30 // Subclass 0B:30 - MIPS
#define PCI_CPU_COPROC                          0x40 // Subclass 0B:40 - Co-processor
#define PCI_CPU_OTHER                           0x80 // Subclass 0B:80 - Other Processor

#define PCI_CLASS_SERBUS_CTRLR                  0x0C // Class 0C: Serial Bus Controller
#define PCI_SBS_1394_CTRLR                      0x00 // Subclass 0C:00 - FireWire (IEEE 1394) Controller
#define PCI_SBS_1394_GENERIC                    0x00 // Subclass 0C:00 Prog IF 00 - Generic IEEE 1394 Controller
#define PCI_SBS_1394_OHCI                       0x10 // Subclass 0C:00 Prog IF 10 - OHCI IEEE 1394 Controller
#define PCI_SBS_ACCESS_CTRLR                    0x01 // Subclass 0C:01 - ACCESS.bus Controller
#define PCI_SBS_SSA                             0x02 // Subclass 0C:02 - SSA
#define PCI_SBS_USB_CTRLR                       0x03 // Subclass 0C:03 - USB Controller
#define PCI_SBS_USB_UHCI                        0x00 // Subclass 0C:03 Prog IF 00 - UHCI Controller
#define PCI_SBS_USB_OHCI                        0x10 // Subclass 0C:03 Prog IF 10 - OHCI Controller
#define PCI_SBS_USB_EHCI                        0x20 // Subclass 0C:03 Prog IF 20 - EHCI Controller
#define PCI_SBS_USB_XHCI                        0x30 // Subclass 0C:03 Prog IF 30 - XHCI Controller
#define PCI_SBS_USB_UNSPECIFIED                 0x80 // Subclass 0C:03 Prog IF 80 - Unspecified USB Controller
#define PCI_SBS_USB_DEVICE                      0xFE // Subclass 0C:03 Prog IF FE - USB Device
#define PCI_SBS_FIBCH_CTRLR                     0x04 // Subclass 0C:04 - Fibre Channel Controller
#define PCI_SBS_SMBUS_CTRLR                     0x05 // Subclass 0C:05 - SMBus Controller
#define PCI_SBS_IFBAND_CTRLR                    0x06 // Subclass 0C:06 - InfiniBand Controller
#define PCI_SBS_IPMI_IF                         0x07 // Subclass 0C:07 - IPMI Interface
#define PCI_SBS_IPMI_SMIC                       0x00 // Subclass 0C:07 Prog IF 00 - SMIC
#define PCI_SBS_IPMI_KBDCTL                     0x01 // Subclass 0C:07 Prog IF 01 - keyboard controller style
#define PCI_SBS_IPMI_BLK                        0x02 // Subclass 0C:07 Prog IF 02 - block transfer
#define PCI_SBS_SERCOS_IF                       0x08 // Subclass 0C:08 - SERCOS (IEC 61491) Interface
#define PCI_SBS_CAN_CTRLR                       0x09 // Subclass 0C:09 - CANbus Controller
#define PCI_SBS_OTHER                           0x80 // Subclass 0C:80 - Other Serial Bus Controller

#define PCI_CLASS_WIRELESS_CTRLR                0x0D // Class 0D: Wireless Controller
#define PCI_WLS_IRDA_CTRLR                      0x00 // Subclass 0D:00 - IRDA-compatible Controller
#define PCI_WLS_CIR_CTRLR                       0x01 // Subclass 0D:01 - Consumer IR Controller
#define PCI_WLS_RF_CTRLR                        0x10 // Subclass 0D:10 - RF Controller
#define PCI_WLS_BT_CTRLR                        0x11 // Subclass 0D:11 - Bluetooth Controller
#define PCI_WLS_BRBAND_CTRLR                    0x12 // Subclass 0D:12 - Broadband Controller
#define PCI_WLS_8021A_CTRLR                     0x20 // Subclass 0D:20 - 802.1a Controller
#define PCI_WLS_8021B_CTRLR                     0x21 // Subclass 0D:21 - 802.1b Controller
#define PCI_WLS_OTHER                           0x80 // Subclass 0D:80 - Other Wireless Controller

#define PCI_CLASS_INTELLIGENT_CTRLR             0x0E // Class 0E: Intelligent Controller
#define PCI_ITL_I20                             0x00 // Subclass 0E:00 - I20 Controller

#define PCI_CLASS_SATCOMM_CTRLR                 0x0F // Class 0F: Satellite Communication Controller
#define PCI_SAT_TV_CTRLR                        0x01 // Subclass 0F:01 - Satellite TV Controller
#define PCI_SAT_AUDIO_CTRLR                     0x02 // Subclass 0F:02 - Satellite Audio Controller
#define PCI_SAT_VOICE_CTRLR                     0x03 // Subclass 0F:03 - Satellite Voice Controller
#define PCI_SAT_DATA_CTRLR                      0x04 // Subclass 0F:04 - Satellite Data Controller

#define PCI_CLASS_ENCRYPT_CTRLR                 0x10 // Class 10: Encryption Controller
#define PCI_ENC_NETCOMP                         0x00 // Subclass 10:00 - Network and Computing Encryption/Decryption
#define PCI_ENC_ENT                             0x10 // Subclass 10:10 - Entertainment Encryption/Decryption
#define PCI_ENC_OTHER                           0x80 // Subclass 10:80 - Other Encryption Controller

#define PCI_CLASS_SIGPROC_CTRLR                 0x11 // Class 11: Signal Processing Controller
#define PCI_SIG_DPIO_MOD                        0x00 // Subclass 11:00 - DPIO Module
#define PCI_SIG_PERF_CNTR                       0x01 // Subclass 11:01 - Performance Counter
#define PCI_SIG_COMM_SYNC                       0x10 // Subclass 11:10 - Communication Synchronizer
#define PCI_SIG_MGMT                            0x20 // Subclass 11:20 - Signal Processing Management
#define PCI_SIG_OTHER                           0x80 // Subclass 11:80 - Other Signal Processing Controller

#define PCI_CLASS_ACCEL                         0x12 // Class 12: Processing Accelerator
#define PCI_CLASS_NE_INSTR                      0x13 // Class 13: Non-essential Instrumentation
#define PCI_CLASS_COPROC                        0x40 // Class 40: Coprocessor

#endif
