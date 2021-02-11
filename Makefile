include $(WORKDIR)/target.mk

# arch makefiles
include $(ARCHDIR)/arch.mk
ifneq ($(ARCHDIR_FAMILY),)
	include $(ARCHDIR_FAMILY)/arch.mk
endif
ifneq ($(ARCHDIR_ARCH),)
	include $(ARCHDIR_ARCH)/arch.mk
endif

# common makefiles
include kernel/objs.mk

OBJS=\
$(TARGET_OBJS) \
$(FAMILY_OBJS) \
$(ARCH_OBJS) \
$(KERNEL_OBJS)

.PHONY: all clean
.SUFFIXES: .o .c .s .asm

all: kernel.elf

kernel.elf: $(OBJS) $(ARCHDIR)/link.ld
	$(CC) -T $(ARCHDIR)/link.ld -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) -c $< -o $@ $(CFLAGS)

.s.o:
	$(AS) -c $< -o $@ $(ASFLAGS)

.asm.o:
	$(ASNG) $< -o $@ $(ASNGFLAGS)

clean:
	rm -f kernel.elf
	rm -f $(OBJS)
