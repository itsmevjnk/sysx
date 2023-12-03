#ifndef TERM_NO_INPUT
#include <hal/keyboard.h>

#ifndef KBD_TERM_ID
#define KBD_TERM_ID         (size_t)-1
#endif

size_t kbd_term_available(const term_hook_t* impl) {
    (void) impl;
    return kbd_char_available(KBD_TERM_ID);
}

char kbd_term_getc(const term_hook_t* impl) {
    (void) impl;
    return kbd_char_read(KBD_TERM_ID);
}
#endif