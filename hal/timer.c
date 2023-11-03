#include <hal/timer.h>

volatile timer_tick_t timer_tick = 0;

void timer_handler(size_t delta) {
    timer_tick += delta;
    // TODO: other timer-related tasks
}