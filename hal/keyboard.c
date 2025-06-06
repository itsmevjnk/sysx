#include <hal/keyboard.h>
#include <hal/keymaps/us_qwerty.h>
#include <kernel/log.h>
#include <string.h>

kbd_info_t kbd_list[KBD_MAXNUM] = {{NULL, 0, 0}};

size_t kbd_register(char* keymap) {
    for(size_t i = 0; i < KBD_MAXNUM; i++) {
        if(!kbd_list[i].in_use) {
            kbd_list[i].in_use++;
            kbd_list[i].keymap = (!keymap) ? (char*)&keymap_us_qwerty : keymap;
            kbd_list[i].mod = 0;
            return i;
        }
    }

    return (size_t)-1;
}

void kbd_unregister(size_t id) {
    if(id < KBD_MAXNUM) kbd_list[id].in_use = 0;
}

/* keyboard event ring buffer */
#ifndef KBD_MAXEVENTS
#define KBD_MAXEVENTS               64
#endif
kbd_event_t kbd_events[KBD_MAXEVENTS];
size_t kbd_events_rdidx = 0; // read index
size_t kbd_events_wridx = 0; // write index

kbd_event_t* kbd_event_peek(size_t id, size_t* rdidx_out, bool last) {
    size_t rdidx = kbd_events_rdidx;
    kbd_event_t* last_event = NULL; size_t last_rdidx = 0;
    while(rdidx != kbd_events_wridx) {
        if((id == (size_t)-1 || kbd_events[rdidx].id == id) && kbd_events[rdidx].code) {
            if(!last) {
                /* return first event */
                if(rdidx_out) *rdidx_out = rdidx; // also return read index if requested
                return &kbd_events[rdidx];
            } else {
                /* return last event - will do later */
                last_rdidx = rdidx;
                last_event = &kbd_events[rdidx];
            }
        }
        rdidx = (rdidx + 1) % KBD_MAXEVENTS;
    }

    if(last && last_event) {
        /* got last event */
        if(rdidx_out) *rdidx_out = last_rdidx;
        return last_event;
    }
    else return NULL; // cannot get anything
}

kbd_event_t* kbd_keypress(size_t id, bool pressed, uint8_t code) {
    if(id >= KBD_MAXNUM || !kbd_list[id].in_use) {
        kdebug("invalid keyboard ID %u", id);
        return NULL;
    }

    if((kbd_events_wridx + 1) % KBD_MAXEVENTS == kbd_events_rdidx) {
        // kdebug("keyboard event buffer is full");
        // return NULL;
        kbd_events_rdidx = (kbd_events_rdidx + 1) & KBD_MAXEVENTS; // discard first event
    }
    
    kbd_event_t* result = &kbd_events[kbd_events_wridx];
    result->id = id;
    result->pressed = (pressed) ? 1 : 0;
    result->code = code;

    /* set modifier mask */
    switch(code) {
        case KEY_LEFTCTRL:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_LCTRL) | ((pressed) ? KEY_MOD_LCTRL : 0);
            break;
        case KEY_RIGHTCTRL:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_RCTRL) | ((pressed) ? KEY_MOD_RCTRL : 0);
            break;
        case KEY_LEFTSHIFT:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_LSHIFT) | ((pressed) ? KEY_MOD_LSHIFT : 0);
            break;
        case KEY_RIGHTSHIFT:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_RSHIFT) | ((pressed) ? KEY_MOD_RSHIFT : 0);
            break;
        case KEY_LEFTALT:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_LALT) | ((pressed) ? KEY_MOD_LALT : 0);
            break;
        case KEY_RIGHTALT:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_RALT) | ((pressed) ? KEY_MOD_RALT : 0);
            break;
        case KEY_LEFTMETA:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_LMETA) | ((pressed) ? KEY_MOD_LMETA : 0);
            break;
        case KEY_RIGHTMETA:
            kbd_list[id].mod = (kbd_list[id].mod & ~KEY_MOD_RMETA) | ((pressed) ? KEY_MOD_RMETA : 0);
            break;
        case KEY_NUMLOCK: // TODO
            if(!pressed) kbd_list[id].mod ^= KEY_MOD_NUM;
            break;
        case KEY_CAPSLOCK: // TODO
            if(!pressed) kbd_list[id].mod ^= KEY_MOD_CAPS;
            break;
    }
    result->mod = kbd_list[id].mod;

    result->c = 0;
    if(pressed) {
        /* process keypress */
        uint8_t flags = ((result->mod & (KEY_MOD_LSHIFT | KEY_MOD_RSHIFT)) ? (1 << 0) : 0) | ((result->mod & KEY_MOD_CAPS) ? (1 << 1) : 0) | ((result->mod & KEY_MOD_RALT) ? (1 << 2) : 0);
        result->c = kbd_list[id].keymap[flags * 256 + code];
    }

    kbd_events_wridx = (kbd_events_wridx + 1) % KBD_MAXEVENTS;

    return result;
}

size_t kbd_event_available(size_t id) {
    size_t result = 0;
    size_t rdidx = kbd_events_rdidx;
    while(rdidx != kbd_events_wridx) {
        if((id == (size_t)-1 || kbd_events[rdidx].id == id) && kbd_events[rdidx].code) result++;
        rdidx = (rdidx + 1) % KBD_MAXEVENTS;
    }
    return result;
}

kbd_event_t* kbd_event_read(size_t id, kbd_event_t* buf) {
    size_t rdidx = 0;
    kbd_event_t* result = kbd_event_peek(id, &rdidx, false); // get first event
    if(!result) return NULL; // no events
    memcpy(buf, result, sizeof(kbd_event_t));

    result->code = 0; // invalidate event
    if(rdidx == kbd_events_rdidx) {
        /* we are reading the first event to be read in the buffer */
        while(!kbd_events[kbd_events_rdidx].code && kbd_events_rdidx != kbd_events_wridx) kbd_events_rdidx = (kbd_events_rdidx + 1) % KBD_MAXEVENTS; // increment and skip invalidated events
    }

    return buf;
}

size_t kbd_char_available(size_t id) {
    size_t result = 0;
    size_t rdidx = kbd_events_rdidx;
    while(rdidx != kbd_events_wridx) {
        if((id == (size_t)-1 || kbd_events[rdidx].id == id) && kbd_events[rdidx].c) result++;
        rdidx = (rdidx + 1) % KBD_MAXEVENTS;
    }
    return result;
}

char kbd_char_read(size_t id) {
    size_t rdidx = kbd_events_rdidx;
    while(rdidx != kbd_events_wridx) {
        if((id == (size_t)-1 || kbd_events[rdidx].id == id) && kbd_events[rdidx].c) {
            /* we've found a character */
            kbd_events[rdidx].code = 0; // invalidate event
            size_t rdidx2 = kbd_events_rdidx;
            while(rdidx2 != rdidx) {
                /* invalidate all preceding events */
                if(id == (size_t)-1 || kbd_events[rdidx2].id == id) {
                    kbd_events[rdidx2].code = 0;
                    if(rdidx2 == kbd_events_rdidx) kbd_events_rdidx = (kbd_events_rdidx + 1) % KBD_MAXEVENTS; 
                }
                rdidx2 = (rdidx2 + 1) % KBD_MAXEVENTS;
            }
            kbd_events_rdidx = (rdidx + 1) % KBD_MAXEVENTS; // set read index to after this event
            return kbd_events[rdidx].c;
        }
        rdidx = (rdidx + 1) % KBD_MAXEVENTS;
    }
    return 0;
}
