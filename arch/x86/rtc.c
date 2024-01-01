#include <arch/x86/rtc.h>
#include <arch/x86/i8259.h>
#include <arch/x86cpu/asm.h>
#include <hal/intr.h>
#include <hal/timer.h>

/* I/O ports */
#define RTC_REG_BASE                0x70
#define RTC_REG_INDEX               (RTC_REG_BASE + 0)
#define RTC_REG_DATA                (RTC_REG_BASE + 1)

/* I/O registers */
#define RTC_REG_SEC                 0x00
#define RTC_REG_MIN                 0x02
#define RTC_REG_HOUR                0x04
#define RTC_REG_DAY                 0x06
#define RTC_REG_DATE                0x07
#define RTC_REG_MONTH               0x08
#define RTC_REG_YEAR                0x09
#define RTC_REG_CENTURY             0x32
#define RTC_REG_STAT_A              0x0A
#define RTC_REG_STAT_B              0x0B
#define RTC_REG_STAT_C              0x0C

/* RTC interrupt line */
#define RTC_IRQ                     8

static inline void rtc_reg_write(uint8_t reg, uint8_t val) {
    outb(RTC_REG_INDEX, reg | (1 << 7)); // disable NMI
    outb(RTC_REG_DATA, val);
}

static inline uint8_t rtc_reg_read(uint8_t reg) {
    outb(RTC_REG_INDEX, reg | (1 << 7)); // disable NMI
    return inb(RTC_REG_DATA);
}

/* status register C bitmasks - indicate which type of interrupt was fired */
#define RTC_SRC_PERIODIC            (1 << 6)
#define RTC_SRC_ALARM               (1 << 5)
#define RTC_SRC_UPDATED             (1 << 4)

static void rtc_irq_handler(size_t irq, void* context) {
    (void) irq;
    uint8_t stat = rtc_reg_read(RTC_REG_STAT_C);
    if(stat & RTC_SRC_PERIODIC) timer_handler(RTC_TIMER_PERIOD, context);
}

void rtc_irq_reset() {
    rtc_reg_read(RTC_REG_STAT_C);
}

void rtc_timer_enable() {
    bool intr_en = intr_test();
    intr_disable();
    rtc_reg_write(RTC_REG_STAT_A, (rtc_reg_read(RTC_REG_STAT_A) & 0xF0) | (RTC_TIMER_RATE & 0x0F)); // set rate
    rtc_reg_write(RTC_REG_STAT_B, rtc_reg_read(RTC_REG_STAT_B) | (1 << 6)); // enable periodic interrupt
    if(intr_en) intr_enable(); // re-enable interrupt
    // rtc_irq_reset();
}

void rtc_timer_disable() {
    bool intr_en = intr_test();
    intr_disable();
    rtc_reg_write(RTC_REG_STAT_B, rtc_reg_read(RTC_REG_STAT_B) & ~(1 << 6));
    if(intr_en) intr_enable(); // re-enable interrupt
    // rtc_irq_reset();
}

void rtc_init() {
    pic_handle(RTC_IRQ, rtc_irq_handler);
    pic_unmask(RTC_IRQ);
}
