#include <hal/timer.h>
#include <exec/task.h>
#include <hal/fbuf.h>

volatile timer_tick_t timer_tick = 0;

void timer_handler(size_t delta, void* context) {
    timer_tick += delta;

    if(fbuf_impl != NULL && fbuf_impl->backbuffer != NULL && !fbuf_impl->dbuf_direct_write && timer_tick - fbuf_impl->tick_flip >= FBUF_FLIP_PERIOD) {
        fbuf_commit();
    }

    if(task_kernel != NULL && (task_current == NULL || timer_tick - task_yield_tick >= TASK_QUANTUM)) {
        task_yield(context);
    }
}

void timer_delay_us(uint64_t us) {
    volatile timer_tick_t t_start = timer_tick;
    while(timer_tick - t_start < us) {
        task_yield_noirq();
    }
}

void timer_delay_ms(uint64_t ms) {
    timer_delay_us(ms * 1000);
}
