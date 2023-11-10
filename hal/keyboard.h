#ifndef HAL_KEYBOARD_H
#define HAL_KEYBOARD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <hal/kbdcodes.h>

/* keyboard event */
typedef struct {
    uint8_t id : 7; // keyboard ID (0-127)
    uint8_t pressed : 1; // 1 - pressed, 0 - released
    uint8_t code; // key scancode
    char c; // decoded character (if applicable; 0 otherwise)
    uint16_t mod; // modifier mask
} __attribute__((packed)) kbd_event_t;

/* keyboard information */
typedef struct {
    char* keymap; // keymap: 8 layers (bit 0: Shift, bit 1: CapsLk, bit 2: AltGr) * 256 scancode to char mappings
    uint8_t in_use; // set if keyboard is in use
} __attribute__ ((packed)) kbd_info_t;

#ifndef KBD_MAXNUM
#define KBD_MAXNUM              128
#endif

extern kbd_info_t kbd_list[KBD_MAXNUM]; // list of keyboards

/*
 * size_t kbd_register(char* keymap)
 *  Registers a new keyboard and returns the keyboard's ID (-1 on failure).
 */
size_t kbd_register(char* keymap);

/*
 * void kbd_unregister(size_t id)
 *  Unregisters an existing keyboard.
 */
void kbd_unregister(size_t id);

/*
 * kbd_event_t* kbd_keypress(size_t id, bool pressed, uint8_t code)
 *  Registers a key press/release event.
 *  Returns the pointer to the event struct on success, or NULL on failure.
 */
kbd_event_t* kbd_keypress(size_t id, bool pressed, uint8_t code);

/*
 * size_t kbd_event_available(size_t id)
 *  Returns the number of keypress events available to be read from the
 *  specified keyboard (or all keyboards if id = -1).
 */
size_t kbd_event_available(size_t id);

/*
 * kbd_event_t* kbd_event_read(size_t id, kbd_event_t* buf)
 *  Retrieves the next keypress event from the specified keyboard (or from
 *  any keyboard if id = -1). Returns NULL if there is none.
 */
kbd_event_t* kbd_event_read(size_t id, kbd_event_t* buf);

/*
 * size_t kbd_char_available(size_t id)
 *  Returns the number of characters available to be read from the specified
 *  keyboard (or all keyboards if id = -1).
 */
size_t kbd_char_available(size_t id);

/*
 * char kbd_char_read(size_t id)
 *  Retrieves the next character from the specified keyboard (or any keyboard
 *  if id = -1). Returns 0 if there is none.
 */
char kbd_char_read(size_t id);

#ifndef TERM_NO_INPUT

#include <hal/terminal.h>

/*
 * size_t kbd_term_available(const term_hook_t* impl)
 *  Terminal HAL available() handler using keyboard.
 */
size_t kbd_term_available(const term_hook_t* impl);

/*
 * char kbd_term_getc(const term_hook_t* impl)
 *  Terminal HAL getc() handler using keyboard.
 */
char kbd_term_getc(const term_hook_t* impl);

#endif

#endif