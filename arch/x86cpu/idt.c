#include <arch/x86cpu/idt.h>
#include <hal/intr.h>
#include <kernel/log.h>
#include <arch/x86cpu/asm.h>
#include <exec/syms.h>
#include <mm/vmm.h>

//#define IDT_DEBUG // uncomment for log hell

/*
 * idt_t
 *  IDT gate entry structure.
 */
typedef struct {
  size_t offset_l : 16; // offset 0:15
  size_t selector : 16;
  size_t zero : 8;
  size_t type : 4;
  size_t storage : 1;
  size_t dpl : 2;
  size_t present : 1;
  size_t offset_h : 16; // offset 16:31
} __attribute__((packed)) idt_t;
idt_t idt_gates[256];

/*
 * idt_desc_t
 *  IDT descriptor table.
 */
typedef struct {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) idt_desc_t;
idt_desc_t idt_desc = {
  sizeof(idt_t) * 256 - 1,
  (uint32_t) &idt_gates
};

void idt_add_gate(uint8_t vector, uint16_t selector, uintptr_t offset, uint8_t type, uint8_t dpl) {
  idt_gates[vector].selector = selector;
  idt_gates[vector].offset_l = offset & 0x0000ffff;
  idt_gates[vector].offset_h = (offset & 0xffff0000) >> 16;
  idt_gates[vector].zero = 0;
  idt_gates[vector].type = type;
  idt_gates[vector].storage = (type == IDT_386_TASK32) ? 1 : 0;
  idt_gates[vector].dpl = dpl;
  idt_gates[vector].present = 1;
#ifdef IDT_DEBUG
  kdebug("vector 0x%02x: 0x%02x:0x%08x, type %u, dpl %u", vector, selector, offset, type, dpl);
#endif
}

/* array of pointers to handler functions */
void (*idt_handlers[256])(uint8_t vector, void* context) = {NULL};

/* functions for intr.h */
void intr_enable() {
  asm("sti");
}

void intr_disable() {
  asm("cli");
}

void intr_handle(uint8_t vector, void (*handler)(uint8_t vector, void* context)) {
  idt_handlers[vector] = handler;
}

/* IDT handler stub to be called by the low level handler portion */
void idt_stub(void* context) {
  idt_context_t* ctxt = context;
  if(idt_handlers[ctxt->vector]) idt_handlers[ctxt->vector](ctxt->vector, context);
}

/* exception handler stub */

/*
 * struct stkframe
 *  Stack frame structure for tracing.
 */
struct stkframe {
  struct stkframe* ebp;
  uint32_t eip;
} __attribute__((packed));

void exc_stub(uint8_t vector, idt_context_t* context) {
  (void) vector;
  
  uint32_t t; // temporary register
  switch(vector) {
	case 0x0E: // page fault
		asm volatile("mov %%cr2, %0" : "=r"(t));
		if(vmm_handle_fault(t, context->exc_code & 0b111)) return;
		break;
	default:
		break;
  }

  /* unhandled exception */
  asm("cli"); // no more interrupts!
  kerror("unhandled exception 0x%x (code 0x%x) @ 0x%x", context->vector, context->exc_code, context->eip);
  kerror("eax=0x%08x ebx=0x%08x ecx=0x%08x edx=0x%08x", context->eax, context->ebx, context->ecx, context->edx);
  kerror("esi=0x%08x edi=0x%08x esp=0x%08x ebp=0x%08x", context->esi, context->edi, context->esp, context->ebp);
  kerror("cs=0x%04x ds=0x%04x es=0x%04x fs=0x%04x gs=0x%04x", context->cs, context->ds, context->es, context->fs, context->gs);
  kerror("user ss=0x%04x user esp=0x%08x eflags=0x%08x", context->ss_usr, context->esp_usr, context->eflags);
#ifdef DEBUG
  kdebug("stack trace:");
  struct stkframe* stk = (struct stkframe*) context->ebp;
  for(size_t i = 0; stk; i++) {
  	struct sym_addr* sym = NULL;
  	if(kernel_syms != NULL) sym = sym_addr2sym(kernel_syms, stk->eip);
	if(sym != NULL) kdebug("%-4u [0x%08x]: 0x%08x (%s + 0x%08x)", i, (uint32_t) stk, stk->eip, sym->sym->name, sym->delta);
    else kdebug("%-4u [0x%08x]: 0x%08x", i, (uint32_t) stk, stk->eip);
    stk = stk->ebp;
  }
#endif
  while(1); // halt now
}

/* giant bomb of code */

/* defined in desctabs.asm */
extern void exc_handler_0();
extern void exc_handler_1();
extern void exc_handler_2();
extern void exc_handler_3();
extern void exc_handler_4();
extern void exc_handler_5();
extern void exc_handler_6();
extern void exc_handler_7();
extern void exc_handler_8();
extern void exc_handler_9();
extern void exc_handler_10();
extern void exc_handler_11();
extern void exc_handler_12();
extern void exc_handler_13();
extern void exc_handler_14();
extern void exc_handler_15();
extern void exc_handler_16();
extern void exc_handler_17();
extern void exc_handler_18();
extern void exc_handler_19();
extern void exc_handler_20();
extern void exc_handler_21();
extern void exc_handler_22();
extern void exc_handler_23();
extern void exc_handler_24();
extern void exc_handler_25();
extern void exc_handler_26();
extern void exc_handler_27();
extern void exc_handler_28();
extern void exc_handler_29();
extern void exc_handler_30();
extern void exc_handler_31();

extern void idt_handler_32();
extern void idt_handler_33();
extern void idt_handler_34();
extern void idt_handler_35();
extern void idt_handler_36();
extern void idt_handler_37();
extern void idt_handler_38();
extern void idt_handler_39();
extern void idt_handler_40();
extern void idt_handler_41();
extern void idt_handler_42();
extern void idt_handler_43();
extern void idt_handler_44();
extern void idt_handler_45();
extern void idt_handler_46();
extern void idt_handler_47();
extern void idt_handler_48();
extern void idt_handler_49();
extern void idt_handler_50();
extern void idt_handler_51();
extern void idt_handler_52();
extern void idt_handler_53();
extern void idt_handler_54();
extern void idt_handler_55();
extern void idt_handler_56();
extern void idt_handler_57();
extern void idt_handler_58();
extern void idt_handler_59();
extern void idt_handler_60();
extern void idt_handler_61();
extern void idt_handler_62();
extern void idt_handler_63();
extern void idt_handler_64();
extern void idt_handler_65();
extern void idt_handler_66();
extern void idt_handler_67();
extern void idt_handler_68();
extern void idt_handler_69();
extern void idt_handler_70();
extern void idt_handler_71();
extern void idt_handler_72();
extern void idt_handler_73();
extern void idt_handler_74();
extern void idt_handler_75();
extern void idt_handler_76();
extern void idt_handler_77();
extern void idt_handler_78();
extern void idt_handler_79();
extern void idt_handler_80();
extern void idt_handler_81();
extern void idt_handler_82();
extern void idt_handler_83();
extern void idt_handler_84();
extern void idt_handler_85();
extern void idt_handler_86();
extern void idt_handler_87();
extern void idt_handler_88();
extern void idt_handler_89();
extern void idt_handler_90();
extern void idt_handler_91();
extern void idt_handler_92();
extern void idt_handler_93();
extern void idt_handler_94();
extern void idt_handler_95();
extern void idt_handler_96();
extern void idt_handler_97();
extern void idt_handler_98();
extern void idt_handler_99();
extern void idt_handler_100();
extern void idt_handler_101();
extern void idt_handler_102();
extern void idt_handler_103();
extern void idt_handler_104();
extern void idt_handler_105();
extern void idt_handler_106();
extern void idt_handler_107();
extern void idt_handler_108();
extern void idt_handler_109();
extern void idt_handler_110();
extern void idt_handler_111();
extern void idt_handler_112();
extern void idt_handler_113();
extern void idt_handler_114();
extern void idt_handler_115();
extern void idt_handler_116();
extern void idt_handler_117();
extern void idt_handler_118();
extern void idt_handler_119();
extern void idt_handler_120();
extern void idt_handler_121();
extern void idt_handler_122();
extern void idt_handler_123();
extern void idt_handler_124();
extern void idt_handler_125();
extern void idt_handler_126();
extern void idt_handler_127();
extern void idt_handler_128();
extern void idt_handler_129();
extern void idt_handler_130();
extern void idt_handler_131();
extern void idt_handler_132();
extern void idt_handler_133();
extern void idt_handler_134();
extern void idt_handler_135();
extern void idt_handler_136();
extern void idt_handler_137();
extern void idt_handler_138();
extern void idt_handler_139();
extern void idt_handler_140();
extern void idt_handler_141();
extern void idt_handler_142();
extern void idt_handler_143();
extern void idt_handler_144();
extern void idt_handler_145();
extern void idt_handler_146();
extern void idt_handler_147();
extern void idt_handler_148();
extern void idt_handler_149();
extern void idt_handler_150();
extern void idt_handler_151();
extern void idt_handler_152();
extern void idt_handler_153();
extern void idt_handler_154();
extern void idt_handler_155();
extern void idt_handler_156();
extern void idt_handler_157();
extern void idt_handler_158();
extern void idt_handler_159();
extern void idt_handler_160();
extern void idt_handler_161();
extern void idt_handler_162();
extern void idt_handler_163();
extern void idt_handler_164();
extern void idt_handler_165();
extern void idt_handler_166();
extern void idt_handler_167();
extern void idt_handler_168();
extern void idt_handler_169();
extern void idt_handler_170();
extern void idt_handler_171();
extern void idt_handler_172();
extern void idt_handler_173();
extern void idt_handler_174();
extern void idt_handler_175();
extern void idt_handler_176();
extern void idt_handler_177();
extern void idt_handler_178();
extern void idt_handler_179();
extern void idt_handler_180();
extern void idt_handler_181();
extern void idt_handler_182();
extern void idt_handler_183();
extern void idt_handler_184();
extern void idt_handler_185();
extern void idt_handler_186();
extern void idt_handler_187();
extern void idt_handler_188();
extern void idt_handler_189();
extern void idt_handler_190();
extern void idt_handler_191();
extern void idt_handler_192();
extern void idt_handler_193();
extern void idt_handler_194();
extern void idt_handler_195();
extern void idt_handler_196();
extern void idt_handler_197();
extern void idt_handler_198();
extern void idt_handler_199();
extern void idt_handler_200();
extern void idt_handler_201();
extern void idt_handler_202();
extern void idt_handler_203();
extern void idt_handler_204();
extern void idt_handler_205();
extern void idt_handler_206();
extern void idt_handler_207();
extern void idt_handler_208();
extern void idt_handler_209();
extern void idt_handler_210();
extern void idt_handler_211();
extern void idt_handler_212();
extern void idt_handler_213();
extern void idt_handler_214();
extern void idt_handler_215();
extern void idt_handler_216();
extern void idt_handler_217();
extern void idt_handler_218();
extern void idt_handler_219();
extern void idt_handler_220();
extern void idt_handler_221();
extern void idt_handler_222();
extern void idt_handler_223();
extern void idt_handler_224();
extern void idt_handler_225();
extern void idt_handler_226();
extern void idt_handler_227();
extern void idt_handler_228();
extern void idt_handler_229();
extern void idt_handler_230();
extern void idt_handler_231();
extern void idt_handler_232();
extern void idt_handler_233();
extern void idt_handler_234();
extern void idt_handler_235();
extern void idt_handler_236();
extern void idt_handler_237();
extern void idt_handler_238();
extern void idt_handler_239();
extern void idt_handler_240();
extern void idt_handler_241();
extern void idt_handler_242();
extern void idt_handler_243();
extern void idt_handler_244();
extern void idt_handler_245();
extern void idt_handler_246();
extern void idt_handler_247();
extern void idt_handler_248();
extern void idt_handler_249();
extern void idt_handler_250();
extern void idt_handler_251();
extern void idt_handler_252();
extern void idt_handler_253();
extern void idt_handler_254();
extern void idt_handler_255();

void idt_init() {
  /* exception handlers */
  idt_add_gate(0, 0x08, (uintptr_t) &exc_handler_0, IDT_386_INTR32, 0);
	idt_add_gate(1, 0x08, (uintptr_t) &exc_handler_1, IDT_386_INTR32, 0);
	idt_add_gate(2, 0x08, (uintptr_t) &exc_handler_2, IDT_386_INTR32, 0);
	idt_add_gate(3, 0x08, (uintptr_t) &exc_handler_3, IDT_386_INTR32, 0);
	idt_add_gate(4, 0x08, (uintptr_t) &exc_handler_4, IDT_386_INTR32, 0);
	idt_add_gate(5, 0x08, (uintptr_t) &exc_handler_5, IDT_386_INTR32, 0);
	idt_add_gate(6, 0x08, (uintptr_t) &exc_handler_6, IDT_386_INTR32, 0);
	idt_add_gate(7, 0x08, (uintptr_t) &exc_handler_7, IDT_386_INTR32, 0);
	idt_add_gate(8, 0x08, (uintptr_t) &exc_handler_8, IDT_386_INTR32, 0);
	idt_add_gate(9, 0x08, (uintptr_t) &exc_handler_9, IDT_386_INTR32, 0);
	idt_add_gate(10, 0x08, (uintptr_t) &exc_handler_10, IDT_386_INTR32, 0);
	idt_add_gate(11, 0x08, (uintptr_t) &exc_handler_11, IDT_386_INTR32, 0);
	idt_add_gate(12, 0x08, (uintptr_t) &exc_handler_12, IDT_386_INTR32, 0);
	idt_add_gate(13, 0x08, (uintptr_t) &exc_handler_13, IDT_386_INTR32, 0);
	idt_add_gate(14, 0x08, (uintptr_t) &exc_handler_14, IDT_386_INTR32, 0);
	idt_add_gate(15, 0x08, (uintptr_t) &exc_handler_15, IDT_386_INTR32, 0);
  idt_add_gate(16, 0x08, (uintptr_t) &exc_handler_16, IDT_386_INTR32, 0);
  idt_add_gate(17, 0x08, (uintptr_t) &exc_handler_17, IDT_386_INTR32, 0);
  idt_add_gate(18, 0x08, (uintptr_t) &exc_handler_18, IDT_386_INTR32, 0);
  idt_add_gate(19, 0x08, (uintptr_t) &exc_handler_19, IDT_386_INTR32, 0);
  idt_add_gate(20, 0x08, (uintptr_t) &exc_handler_20, IDT_386_INTR32, 0);
  idt_add_gate(21, 0x08, (uintptr_t) &exc_handler_21, IDT_386_INTR32, 0);
  idt_add_gate(22, 0x08, (uintptr_t) &exc_handler_22, IDT_386_INTR32, 0);
  idt_add_gate(23, 0x08, (uintptr_t) &exc_handler_23, IDT_386_INTR32, 0);
  idt_add_gate(24, 0x08, (uintptr_t) &exc_handler_24, IDT_386_INTR32, 0);
  idt_add_gate(25, 0x08, (uintptr_t) &exc_handler_25, IDT_386_INTR32, 0);
  idt_add_gate(26, 0x08, (uintptr_t) &exc_handler_26, IDT_386_INTR32, 0);
  idt_add_gate(27, 0x08, (uintptr_t) &exc_handler_27, IDT_386_INTR32, 0);
  idt_add_gate(28, 0x08, (uintptr_t) &exc_handler_28, IDT_386_INTR32, 0);
  idt_add_gate(29, 0x08, (uintptr_t) &exc_handler_29, IDT_386_INTR32, 0);
  idt_add_gate(30, 0x08, (uintptr_t) &exc_handler_30, IDT_386_INTR32, 0);
  idt_add_gate(31, 0x08, (uintptr_t) &exc_handler_31, IDT_386_INTR32, 0);
  for(uint8_t i = 0; i < 0x20; i++) idt_handlers[i] = (void*)&exc_stub;

  /* other IDT handlers */
	idt_add_gate(32, 0x08, (uintptr_t) &idt_handler_32, IDT_386_INTR32, 3);
	idt_add_gate(33, 0x08, (uintptr_t) &idt_handler_33, IDT_386_INTR32, 3);
	idt_add_gate(34, 0x08, (uintptr_t) &idt_handler_34, IDT_386_INTR32, 3);
	idt_add_gate(35, 0x08, (uintptr_t) &idt_handler_35, IDT_386_INTR32, 3);
	idt_add_gate(36, 0x08, (uintptr_t) &idt_handler_36, IDT_386_INTR32, 3);
	idt_add_gate(37, 0x08, (uintptr_t) &idt_handler_37, IDT_386_INTR32, 3);
	idt_add_gate(38, 0x08, (uintptr_t) &idt_handler_38, IDT_386_INTR32, 3);
	idt_add_gate(39, 0x08, (uintptr_t) &idt_handler_39, IDT_386_INTR32, 3);
	idt_add_gate(40, 0x08, (uintptr_t) &idt_handler_40, IDT_386_INTR32, 3);
	idt_add_gate(41, 0x08, (uintptr_t) &idt_handler_41, IDT_386_INTR32, 3);
	idt_add_gate(42, 0x08, (uintptr_t) &idt_handler_42, IDT_386_INTR32, 3);
	idt_add_gate(43, 0x08, (uintptr_t) &idt_handler_43, IDT_386_INTR32, 3);
	idt_add_gate(44, 0x08, (uintptr_t) &idt_handler_44, IDT_386_INTR32, 3);
	idt_add_gate(45, 0x08, (uintptr_t) &idt_handler_45, IDT_386_INTR32, 3);
	idt_add_gate(46, 0x08, (uintptr_t) &idt_handler_46, IDT_386_INTR32, 3);
	idt_add_gate(47, 0x08, (uintptr_t) &idt_handler_47, IDT_386_INTR32, 3);
	idt_add_gate(48, 0x08, (uintptr_t) &idt_handler_48, IDT_386_INTR32, 3);
	idt_add_gate(49, 0x08, (uintptr_t) &idt_handler_49, IDT_386_INTR32, 3);
	idt_add_gate(50, 0x08, (uintptr_t) &idt_handler_50, IDT_386_INTR32, 3);
	idt_add_gate(51, 0x08, (uintptr_t) &idt_handler_51, IDT_386_INTR32, 3);
	idt_add_gate(52, 0x08, (uintptr_t) &idt_handler_52, IDT_386_INTR32, 3);
	idt_add_gate(53, 0x08, (uintptr_t) &idt_handler_53, IDT_386_INTR32, 3);
	idt_add_gate(54, 0x08, (uintptr_t) &idt_handler_54, IDT_386_INTR32, 3);
	idt_add_gate(55, 0x08, (uintptr_t) &idt_handler_55, IDT_386_INTR32, 3);
	idt_add_gate(56, 0x08, (uintptr_t) &idt_handler_56, IDT_386_INTR32, 3);
	idt_add_gate(57, 0x08, (uintptr_t) &idt_handler_57, IDT_386_INTR32, 3);
	idt_add_gate(58, 0x08, (uintptr_t) &idt_handler_58, IDT_386_INTR32, 3);
	idt_add_gate(59, 0x08, (uintptr_t) &idt_handler_59, IDT_386_INTR32, 3);
	idt_add_gate(60, 0x08, (uintptr_t) &idt_handler_60, IDT_386_INTR32, 3);
	idt_add_gate(61, 0x08, (uintptr_t) &idt_handler_61, IDT_386_INTR32, 3);
	idt_add_gate(62, 0x08, (uintptr_t) &idt_handler_62, IDT_386_INTR32, 3);
	idt_add_gate(63, 0x08, (uintptr_t) &idt_handler_63, IDT_386_INTR32, 3);
	idt_add_gate(64, 0x08, (uintptr_t) &idt_handler_64, IDT_386_INTR32, 3);
	idt_add_gate(65, 0x08, (uintptr_t) &idt_handler_65, IDT_386_INTR32, 3);
	idt_add_gate(66, 0x08, (uintptr_t) &idt_handler_66, IDT_386_INTR32, 3);
	idt_add_gate(67, 0x08, (uintptr_t) &idt_handler_67, IDT_386_INTR32, 3);
	idt_add_gate(68, 0x08, (uintptr_t) &idt_handler_68, IDT_386_INTR32, 3);
	idt_add_gate(69, 0x08, (uintptr_t) &idt_handler_69, IDT_386_INTR32, 3);
	idt_add_gate(70, 0x08, (uintptr_t) &idt_handler_70, IDT_386_INTR32, 3);
	idt_add_gate(71, 0x08, (uintptr_t) &idt_handler_71, IDT_386_INTR32, 3);
	idt_add_gate(72, 0x08, (uintptr_t) &idt_handler_72, IDT_386_INTR32, 3);
	idt_add_gate(73, 0x08, (uintptr_t) &idt_handler_73, IDT_386_INTR32, 3);
	idt_add_gate(74, 0x08, (uintptr_t) &idt_handler_74, IDT_386_INTR32, 3);
	idt_add_gate(75, 0x08, (uintptr_t) &idt_handler_75, IDT_386_INTR32, 3);
	idt_add_gate(76, 0x08, (uintptr_t) &idt_handler_76, IDT_386_INTR32, 3);
	idt_add_gate(77, 0x08, (uintptr_t) &idt_handler_77, IDT_386_INTR32, 3);
	idt_add_gate(78, 0x08, (uintptr_t) &idt_handler_78, IDT_386_INTR32, 3);
	idt_add_gate(79, 0x08, (uintptr_t) &idt_handler_79, IDT_386_INTR32, 3);
	idt_add_gate(80, 0x08, (uintptr_t) &idt_handler_80, IDT_386_INTR32, 3);
	idt_add_gate(81, 0x08, (uintptr_t) &idt_handler_81, IDT_386_INTR32, 3);
	idt_add_gate(82, 0x08, (uintptr_t) &idt_handler_82, IDT_386_INTR32, 3);
	idt_add_gate(83, 0x08, (uintptr_t) &idt_handler_83, IDT_386_INTR32, 3);
	idt_add_gate(84, 0x08, (uintptr_t) &idt_handler_84, IDT_386_INTR32, 3);
	idt_add_gate(85, 0x08, (uintptr_t) &idt_handler_85, IDT_386_INTR32, 3);
	idt_add_gate(86, 0x08, (uintptr_t) &idt_handler_86, IDT_386_INTR32, 3);
	idt_add_gate(87, 0x08, (uintptr_t) &idt_handler_87, IDT_386_INTR32, 3);
	idt_add_gate(88, 0x08, (uintptr_t) &idt_handler_88, IDT_386_INTR32, 3);
	idt_add_gate(89, 0x08, (uintptr_t) &idt_handler_89, IDT_386_INTR32, 3);
	idt_add_gate(90, 0x08, (uintptr_t) &idt_handler_90, IDT_386_INTR32, 3);
	idt_add_gate(91, 0x08, (uintptr_t) &idt_handler_91, IDT_386_INTR32, 3);
	idt_add_gate(92, 0x08, (uintptr_t) &idt_handler_92, IDT_386_INTR32, 3);
	idt_add_gate(93, 0x08, (uintptr_t) &idt_handler_93, IDT_386_INTR32, 3);
	idt_add_gate(94, 0x08, (uintptr_t) &idt_handler_94, IDT_386_INTR32, 3);
	idt_add_gate(95, 0x08, (uintptr_t) &idt_handler_95, IDT_386_INTR32, 3);
	idt_add_gate(96, 0x08, (uintptr_t) &idt_handler_96, IDT_386_INTR32, 3);
	idt_add_gate(97, 0x08, (uintptr_t) &idt_handler_97, IDT_386_INTR32, 3);
	idt_add_gate(98, 0x08, (uintptr_t) &idt_handler_98, IDT_386_INTR32, 3);
	idt_add_gate(99, 0x08, (uintptr_t) &idt_handler_99, IDT_386_INTR32, 3);
	idt_add_gate(100, 0x08, (uintptr_t) &idt_handler_100, IDT_386_INTR32, 3);
	idt_add_gate(101, 0x08, (uintptr_t) &idt_handler_101, IDT_386_INTR32, 3);
	idt_add_gate(102, 0x08, (uintptr_t) &idt_handler_102, IDT_386_INTR32, 3);
	idt_add_gate(103, 0x08, (uintptr_t) &idt_handler_103, IDT_386_INTR32, 3);
	idt_add_gate(104, 0x08, (uintptr_t) &idt_handler_104, IDT_386_INTR32, 3);
	idt_add_gate(105, 0x08, (uintptr_t) &idt_handler_105, IDT_386_INTR32, 3);
	idt_add_gate(106, 0x08, (uintptr_t) &idt_handler_106, IDT_386_INTR32, 3);
	idt_add_gate(107, 0x08, (uintptr_t) &idt_handler_107, IDT_386_INTR32, 3);
	idt_add_gate(108, 0x08, (uintptr_t) &idt_handler_108, IDT_386_INTR32, 3);
	idt_add_gate(109, 0x08, (uintptr_t) &idt_handler_109, IDT_386_INTR32, 3);
	idt_add_gate(110, 0x08, (uintptr_t) &idt_handler_110, IDT_386_INTR32, 3);
	idt_add_gate(111, 0x08, (uintptr_t) &idt_handler_111, IDT_386_INTR32, 3);
	idt_add_gate(112, 0x08, (uintptr_t) &idt_handler_112, IDT_386_INTR32, 3);
	idt_add_gate(113, 0x08, (uintptr_t) &idt_handler_113, IDT_386_INTR32, 3);
	idt_add_gate(114, 0x08, (uintptr_t) &idt_handler_114, IDT_386_INTR32, 3);
	idt_add_gate(115, 0x08, (uintptr_t) &idt_handler_115, IDT_386_INTR32, 3);
	idt_add_gate(116, 0x08, (uintptr_t) &idt_handler_116, IDT_386_INTR32, 3);
	idt_add_gate(117, 0x08, (uintptr_t) &idt_handler_117, IDT_386_INTR32, 3);
	idt_add_gate(118, 0x08, (uintptr_t) &idt_handler_118, IDT_386_INTR32, 3);
	idt_add_gate(119, 0x08, (uintptr_t) &idt_handler_119, IDT_386_INTR32, 3);
	idt_add_gate(120, 0x08, (uintptr_t) &idt_handler_120, IDT_386_INTR32, 3);
	idt_add_gate(121, 0x08, (uintptr_t) &idt_handler_121, IDT_386_INTR32, 3);
	idt_add_gate(122, 0x08, (uintptr_t) &idt_handler_122, IDT_386_INTR32, 3);
	idt_add_gate(123, 0x08, (uintptr_t) &idt_handler_123, IDT_386_INTR32, 3);
	idt_add_gate(124, 0x08, (uintptr_t) &idt_handler_124, IDT_386_INTR32, 3);
	idt_add_gate(125, 0x08, (uintptr_t) &idt_handler_125, IDT_386_INTR32, 3);
	idt_add_gate(126, 0x08, (uintptr_t) &idt_handler_126, IDT_386_INTR32, 3);
	idt_add_gate(127, 0x08, (uintptr_t) &idt_handler_127, IDT_386_INTR32, 3);
	idt_add_gate(128, 0x08, (uintptr_t) &idt_handler_128, IDT_386_INTR32, 3);
	idt_add_gate(129, 0x08, (uintptr_t) &idt_handler_129, IDT_386_INTR32, 3);
	idt_add_gate(130, 0x08, (uintptr_t) &idt_handler_130, IDT_386_INTR32, 3);
	idt_add_gate(131, 0x08, (uintptr_t) &idt_handler_131, IDT_386_INTR32, 3);
	idt_add_gate(132, 0x08, (uintptr_t) &idt_handler_132, IDT_386_INTR32, 3);
	idt_add_gate(133, 0x08, (uintptr_t) &idt_handler_133, IDT_386_INTR32, 3);
	idt_add_gate(134, 0x08, (uintptr_t) &idt_handler_134, IDT_386_INTR32, 3);
	idt_add_gate(135, 0x08, (uintptr_t) &idt_handler_135, IDT_386_INTR32, 3);
	idt_add_gate(136, 0x08, (uintptr_t) &idt_handler_136, IDT_386_INTR32, 3);
	idt_add_gate(137, 0x08, (uintptr_t) &idt_handler_137, IDT_386_INTR32, 3);
	idt_add_gate(138, 0x08, (uintptr_t) &idt_handler_138, IDT_386_INTR32, 3);
	idt_add_gate(139, 0x08, (uintptr_t) &idt_handler_139, IDT_386_INTR32, 3);
	idt_add_gate(140, 0x08, (uintptr_t) &idt_handler_140, IDT_386_INTR32, 3);
	idt_add_gate(141, 0x08, (uintptr_t) &idt_handler_141, IDT_386_INTR32, 3);
	idt_add_gate(142, 0x08, (uintptr_t) &idt_handler_142, IDT_386_INTR32, 3);
	idt_add_gate(143, 0x08, (uintptr_t) &idt_handler_143, IDT_386_INTR32, 3);
	idt_add_gate(144, 0x08, (uintptr_t) &idt_handler_144, IDT_386_INTR32, 3);
	idt_add_gate(145, 0x08, (uintptr_t) &idt_handler_145, IDT_386_INTR32, 3);
	idt_add_gate(146, 0x08, (uintptr_t) &idt_handler_146, IDT_386_INTR32, 3);
	idt_add_gate(147, 0x08, (uintptr_t) &idt_handler_147, IDT_386_INTR32, 3);
	idt_add_gate(148, 0x08, (uintptr_t) &idt_handler_148, IDT_386_INTR32, 3);
	idt_add_gate(149, 0x08, (uintptr_t) &idt_handler_149, IDT_386_INTR32, 3);
	idt_add_gate(150, 0x08, (uintptr_t) &idt_handler_150, IDT_386_INTR32, 3);
	idt_add_gate(151, 0x08, (uintptr_t) &idt_handler_151, IDT_386_INTR32, 3);
	idt_add_gate(152, 0x08, (uintptr_t) &idt_handler_152, IDT_386_INTR32, 3);
	idt_add_gate(153, 0x08, (uintptr_t) &idt_handler_153, IDT_386_INTR32, 3);
	idt_add_gate(154, 0x08, (uintptr_t) &idt_handler_154, IDT_386_INTR32, 3);
	idt_add_gate(155, 0x08, (uintptr_t) &idt_handler_155, IDT_386_INTR32, 3);
	idt_add_gate(156, 0x08, (uintptr_t) &idt_handler_156, IDT_386_INTR32, 3);
	idt_add_gate(157, 0x08, (uintptr_t) &idt_handler_157, IDT_386_INTR32, 3);
	idt_add_gate(158, 0x08, (uintptr_t) &idt_handler_158, IDT_386_INTR32, 3);
	idt_add_gate(159, 0x08, (uintptr_t) &idt_handler_159, IDT_386_INTR32, 3);
	idt_add_gate(160, 0x08, (uintptr_t) &idt_handler_160, IDT_386_INTR32, 3);
	idt_add_gate(161, 0x08, (uintptr_t) &idt_handler_161, IDT_386_INTR32, 3);
	idt_add_gate(162, 0x08, (uintptr_t) &idt_handler_162, IDT_386_INTR32, 3);
	idt_add_gate(163, 0x08, (uintptr_t) &idt_handler_163, IDT_386_INTR32, 3);
	idt_add_gate(164, 0x08, (uintptr_t) &idt_handler_164, IDT_386_INTR32, 3);
	idt_add_gate(165, 0x08, (uintptr_t) &idt_handler_165, IDT_386_INTR32, 3);
	idt_add_gate(166, 0x08, (uintptr_t) &idt_handler_166, IDT_386_INTR32, 3);
	idt_add_gate(167, 0x08, (uintptr_t) &idt_handler_167, IDT_386_INTR32, 3);
	idt_add_gate(168, 0x08, (uintptr_t) &idt_handler_168, IDT_386_INTR32, 3);
	idt_add_gate(169, 0x08, (uintptr_t) &idt_handler_169, IDT_386_INTR32, 3);
	idt_add_gate(170, 0x08, (uintptr_t) &idt_handler_170, IDT_386_INTR32, 3);
	idt_add_gate(171, 0x08, (uintptr_t) &idt_handler_171, IDT_386_INTR32, 3);
	idt_add_gate(172, 0x08, (uintptr_t) &idt_handler_172, IDT_386_INTR32, 3);
	idt_add_gate(173, 0x08, (uintptr_t) &idt_handler_173, IDT_386_INTR32, 3);
	idt_add_gate(174, 0x08, (uintptr_t) &idt_handler_174, IDT_386_INTR32, 3);
	idt_add_gate(175, 0x08, (uintptr_t) &idt_handler_175, IDT_386_INTR32, 3);
	idt_add_gate(176, 0x08, (uintptr_t) &idt_handler_176, IDT_386_INTR32, 3);
	idt_add_gate(177, 0x08, (uintptr_t) &idt_handler_177, IDT_386_INTR32, 3);
	idt_add_gate(178, 0x08, (uintptr_t) &idt_handler_178, IDT_386_INTR32, 3);
	idt_add_gate(179, 0x08, (uintptr_t) &idt_handler_179, IDT_386_INTR32, 3);
	idt_add_gate(180, 0x08, (uintptr_t) &idt_handler_180, IDT_386_INTR32, 3);
	idt_add_gate(181, 0x08, (uintptr_t) &idt_handler_181, IDT_386_INTR32, 3);
	idt_add_gate(182, 0x08, (uintptr_t) &idt_handler_182, IDT_386_INTR32, 3);
	idt_add_gate(183, 0x08, (uintptr_t) &idt_handler_183, IDT_386_INTR32, 3);
	idt_add_gate(184, 0x08, (uintptr_t) &idt_handler_184, IDT_386_INTR32, 3);
	idt_add_gate(185, 0x08, (uintptr_t) &idt_handler_185, IDT_386_INTR32, 3);
	idt_add_gate(186, 0x08, (uintptr_t) &idt_handler_186, IDT_386_INTR32, 3);
	idt_add_gate(187, 0x08, (uintptr_t) &idt_handler_187, IDT_386_INTR32, 3);
	idt_add_gate(188, 0x08, (uintptr_t) &idt_handler_188, IDT_386_INTR32, 3);
	idt_add_gate(189, 0x08, (uintptr_t) &idt_handler_189, IDT_386_INTR32, 3);
	idt_add_gate(190, 0x08, (uintptr_t) &idt_handler_190, IDT_386_INTR32, 3);
	idt_add_gate(191, 0x08, (uintptr_t) &idt_handler_191, IDT_386_INTR32, 3);
	idt_add_gate(192, 0x08, (uintptr_t) &idt_handler_192, IDT_386_INTR32, 3);
	idt_add_gate(193, 0x08, (uintptr_t) &idt_handler_193, IDT_386_INTR32, 3);
	idt_add_gate(194, 0x08, (uintptr_t) &idt_handler_194, IDT_386_INTR32, 3);
	idt_add_gate(195, 0x08, (uintptr_t) &idt_handler_195, IDT_386_INTR32, 3);
	idt_add_gate(196, 0x08, (uintptr_t) &idt_handler_196, IDT_386_INTR32, 3);
	idt_add_gate(197, 0x08, (uintptr_t) &idt_handler_197, IDT_386_INTR32, 3);
	idt_add_gate(198, 0x08, (uintptr_t) &idt_handler_198, IDT_386_INTR32, 3);
	idt_add_gate(199, 0x08, (uintptr_t) &idt_handler_199, IDT_386_INTR32, 3);
	idt_add_gate(200, 0x08, (uintptr_t) &idt_handler_200, IDT_386_INTR32, 3);
	idt_add_gate(201, 0x08, (uintptr_t) &idt_handler_201, IDT_386_INTR32, 3);
	idt_add_gate(202, 0x08, (uintptr_t) &idt_handler_202, IDT_386_INTR32, 3);
	idt_add_gate(203, 0x08, (uintptr_t) &idt_handler_203, IDT_386_INTR32, 3);
	idt_add_gate(204, 0x08, (uintptr_t) &idt_handler_204, IDT_386_INTR32, 3);
	idt_add_gate(205, 0x08, (uintptr_t) &idt_handler_205, IDT_386_INTR32, 3);
	idt_add_gate(206, 0x08, (uintptr_t) &idt_handler_206, IDT_386_INTR32, 3);
	idt_add_gate(207, 0x08, (uintptr_t) &idt_handler_207, IDT_386_INTR32, 3);
	idt_add_gate(208, 0x08, (uintptr_t) &idt_handler_208, IDT_386_INTR32, 3);
	idt_add_gate(209, 0x08, (uintptr_t) &idt_handler_209, IDT_386_INTR32, 3);
	idt_add_gate(210, 0x08, (uintptr_t) &idt_handler_210, IDT_386_INTR32, 3);
	idt_add_gate(211, 0x08, (uintptr_t) &idt_handler_211, IDT_386_INTR32, 3);
	idt_add_gate(212, 0x08, (uintptr_t) &idt_handler_212, IDT_386_INTR32, 3);
	idt_add_gate(213, 0x08, (uintptr_t) &idt_handler_213, IDT_386_INTR32, 3);
	idt_add_gate(214, 0x08, (uintptr_t) &idt_handler_214, IDT_386_INTR32, 3);
	idt_add_gate(215, 0x08, (uintptr_t) &idt_handler_215, IDT_386_INTR32, 3);
	idt_add_gate(216, 0x08, (uintptr_t) &idt_handler_216, IDT_386_INTR32, 3);
	idt_add_gate(217, 0x08, (uintptr_t) &idt_handler_217, IDT_386_INTR32, 3);
	idt_add_gate(218, 0x08, (uintptr_t) &idt_handler_218, IDT_386_INTR32, 3);
	idt_add_gate(219, 0x08, (uintptr_t) &idt_handler_219, IDT_386_INTR32, 3);
	idt_add_gate(220, 0x08, (uintptr_t) &idt_handler_220, IDT_386_INTR32, 3);
	idt_add_gate(221, 0x08, (uintptr_t) &idt_handler_221, IDT_386_INTR32, 3);
	idt_add_gate(222, 0x08, (uintptr_t) &idt_handler_222, IDT_386_INTR32, 3);
	idt_add_gate(223, 0x08, (uintptr_t) &idt_handler_223, IDT_386_INTR32, 3);
	idt_add_gate(224, 0x08, (uintptr_t) &idt_handler_224, IDT_386_INTR32, 3);
	idt_add_gate(225, 0x08, (uintptr_t) &idt_handler_225, IDT_386_INTR32, 3);
	idt_add_gate(226, 0x08, (uintptr_t) &idt_handler_226, IDT_386_INTR32, 3);
	idt_add_gate(227, 0x08, (uintptr_t) &idt_handler_227, IDT_386_INTR32, 3);
	idt_add_gate(228, 0x08, (uintptr_t) &idt_handler_228, IDT_386_INTR32, 3);
	idt_add_gate(229, 0x08, (uintptr_t) &idt_handler_229, IDT_386_INTR32, 3);
	idt_add_gate(230, 0x08, (uintptr_t) &idt_handler_230, IDT_386_INTR32, 3);
	idt_add_gate(231, 0x08, (uintptr_t) &idt_handler_231, IDT_386_INTR32, 3);
	idt_add_gate(232, 0x08, (uintptr_t) &idt_handler_232, IDT_386_INTR32, 3);
	idt_add_gate(233, 0x08, (uintptr_t) &idt_handler_233, IDT_386_INTR32, 3);
	idt_add_gate(234, 0x08, (uintptr_t) &idt_handler_234, IDT_386_INTR32, 3);
	idt_add_gate(235, 0x08, (uintptr_t) &idt_handler_235, IDT_386_INTR32, 3);
	idt_add_gate(236, 0x08, (uintptr_t) &idt_handler_236, IDT_386_INTR32, 3);
	idt_add_gate(237, 0x08, (uintptr_t) &idt_handler_237, IDT_386_INTR32, 3);
	idt_add_gate(238, 0x08, (uintptr_t) &idt_handler_238, IDT_386_INTR32, 3);
	idt_add_gate(239, 0x08, (uintptr_t) &idt_handler_239, IDT_386_INTR32, 3);
	idt_add_gate(240, 0x08, (uintptr_t) &idt_handler_240, IDT_386_INTR32, 3);
	idt_add_gate(241, 0x08, (uintptr_t) &idt_handler_241, IDT_386_INTR32, 3);
	idt_add_gate(242, 0x08, (uintptr_t) &idt_handler_242, IDT_386_INTR32, 3);
	idt_add_gate(243, 0x08, (uintptr_t) &idt_handler_243, IDT_386_INTR32, 3);
	idt_add_gate(244, 0x08, (uintptr_t) &idt_handler_244, IDT_386_INTR32, 3);
	idt_add_gate(245, 0x08, (uintptr_t) &idt_handler_245, IDT_386_INTR32, 3);
	idt_add_gate(246, 0x08, (uintptr_t) &idt_handler_246, IDT_386_INTR32, 3);
	idt_add_gate(247, 0x08, (uintptr_t) &idt_handler_247, IDT_386_INTR32, 3);
	idt_add_gate(248, 0x08, (uintptr_t) &idt_handler_248, IDT_386_INTR32, 3);
	idt_add_gate(249, 0x08, (uintptr_t) &idt_handler_249, IDT_386_INTR32, 3);
	idt_add_gate(250, 0x08, (uintptr_t) &idt_handler_250, IDT_386_INTR32, 3);
	idt_add_gate(251, 0x08, (uintptr_t) &idt_handler_251, IDT_386_INTR32, 3);
	idt_add_gate(252, 0x08, (uintptr_t) &idt_handler_252, IDT_386_INTR32, 3);
	idt_add_gate(253, 0x08, (uintptr_t) &idt_handler_253, IDT_386_INTR32, 3);
	idt_add_gate(254, 0x08, (uintptr_t) &idt_handler_254, IDT_386_INTR32, 3);
	idt_add_gate(255, 0x08, (uintptr_t) &idt_handler_255, IDT_386_INTR32, 3);

	__asm__ volatile("lidt %0" : : "m"(idt_desc)); // load IDTR

	// outb(0xA0, 0x20); outb(0x20, 0x20);
	// outb(0xA1, 0xFF); outb(0x21, 0xFF); // disable PIC for the time being so we don't end up with IRQs showing up as exceptions
	__asm__ volatile("sti");
}
