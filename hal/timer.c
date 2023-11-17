#include <hal/timer.h>
#include <exec/task.h>

volatile timer_tick_t timer_tick = 0;

void timer_handler(size_t delta, void* context) {
    timer_tick += delta;

    task_yield(context);
}