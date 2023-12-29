#ifndef ARCH_X86_RTC_H
#define ARCH_X86_RTC_H

#include <stddef.h>
#include <stdint.h>

/* RTC TIMER INTERRUPT RATE */
#ifndef RTC_TIMER_RATE
#define RTC_TIMER_RATE                      3 // 3-15: 3 = 8.192kHz, 4 = 4.096kHz, 5 = 2.048kHz, etc.
#endif
#define RTC_TIMER_FREQ                      (32768 >> (RTC_TIMER_RATE - 1))
#define RTC_TIMER_PERIOD                    (1000000UL / RTC_TIMER_FREQ) // in timer ticks (microseconds)

/*
 * void rtc_timer_enable()
 *  Enables the RTC's periodic timer interrupt as a timer tick
 *  increment source.
 *  NOTE: Any other timer tick source MUST BE DISABLED!
 */
void rtc_timer_enable();

/*
 * void rtc_timer_disable()
 *  Disables the RTC's periodic timer interrupt.
 */
void rtc_timer_disable();

/*
 * void rtc_init()
 *  Installs the RTC interrupt handler.
 */
void rtc_init();

/*
 * void rtc_irq_reset()
 *  Resets the RTC interrupt, so that it continues to fire interrupts.
 */
void rtc_irq_reset();

#endif
