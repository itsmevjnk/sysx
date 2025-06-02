#include <hal/fbuf.h>
#include <mm/vmm.h>
#include <string.h>
#include <hal/intr.h>
#include <kernel/log.h>

fbuf_t* fbuf_impl = NULL;

size_t fbuf_bytes_per_pixel(const fbuf_t* impl) {
    if(!impl) impl = fbuf_impl;
    switch(impl->type) {
        case FBUF_15BPP_BGR555:
            return 2;
        case FBUF_15BPP_BGR555_RE:
            return 2;
        case FBUF_15BPP_RGB555:
            return 2;
        case FBUF_15BPP_RGB555_RE:
            return 2;
        case FBUF_16BPP_BGR565:
            return 2;
        case FBUF_16BPP_BGR565_RE:
            return 2;
        case FBUF_16BPP_RGB565:
            return 2;
        case FBUF_16BPP_RGB565_RE:
            return 2;
        case FBUF_24BPP_BGR888:
            return 3;
        case FBUF_24BPP_RGB888:
            return 3;
        case FBUF_32BPP_BGR888:
            return 4;
        case FBUF_32BPP_BGR888_RE:
            return 4;
        case FBUF_32BPP_RGB888:
            return 4;
        case FBUF_32BPP_RGB888_RE:
            return 4;
        default:
            return 0;
    }
}

size_t fbuf_process_color(uint32_t* color) {
    *color &= 0x00FFFFFF;
    switch(fbuf_impl->type) {
        case FBUF_15BPP_BGR555:
            if(color)
                *color = ((FBUF_B(*color) >> 3) << 10) | ((FBUF_G(*color) >> 3) << 5) | ((FBUF_R(*color) >> 3) << 0);
            return 2;
        case FBUF_15BPP_BGR555_RE:
            if(color) {
                *color = ((FBUF_B(*color) >> 3) << 10) | ((FBUF_G(*color) >> 3) << 5) | ((FBUF_R(*color) >> 3) << 0);
                *color = ((*color & 0x00FF) << 8) | ((*color & 0xFF00) >> 8);
            }
            return 2;
        case FBUF_15BPP_RGB555:
            if(color)
                *color = ((FBUF_R(*color) >> 3) << 10) | ((FBUF_G(*color) >> 3) << 5) | ((FBUF_B(*color) >> 3) << 0);
            return 2;
        case FBUF_15BPP_RGB555_RE:
            if(color) {
                *color = ((FBUF_R(*color) >> 3) << 10) | ((FBUF_G(*color) >> 3) << 5) | ((FBUF_B(*color) >> 3) << 0);
                *color = ((*color & 0x00FF) << 8) | ((*color & 0xFF00) >> 8);
            }
            return 2;
        case FBUF_16BPP_BGR565:
            if(color)
                *color = (((FBUF_B(*color) >> 3) << 11) | ((FBUF_G(*color) >> 2) << 5) | ((FBUF_R(*color) >> 3) << 0));
            return 2;
        case FBUF_16BPP_BGR565_RE:
            if(color) {
                *color = ((FBUF_B(*color) >> 3) << 11) | ((FBUF_G(*color) >> 2) << 5) | ((FBUF_R(*color) >> 3) << 0);
                *color = ((*color & 0x00FF) << 8) | ((*color & 0xFF00) >> 8);
            }
            return 2;
        case FBUF_16BPP_RGB565:
            if(color)
                *color = ((FBUF_R(*color) >> 3) << 11) | ((FBUF_G(*color) >> 2) << 5) | ((FBUF_B(*color) >> 3) << 0);
            return 2;
        case FBUF_16BPP_RGB565_RE:
            if(color) {
                *color = ((FBUF_R(*color) >> 3) << 11) | ((FBUF_G(*color) >> 2) << 5) | ((FBUF_B(*color) >> 3) << 0);
                *color = ((*color & 0x00FF) << 8) | ((*color & 0xFF00) >> 8);
            }
            return 2;
        case FBUF_24BPP_BGR888:
            return 3;
        case FBUF_24BPP_RGB888:
            return 3;
        case FBUF_32BPP_BGR888:
            if(color)
                *color = (FBUF_R(*color) << 0) | (FBUF_G(*color) << 8) | (FBUF_B(*color) << 16);
            return 4;
        case FBUF_32BPP_BGR888_RE:
            if(color)
                *color = (FBUF_R(*color) << 24) | (FBUF_G(*color) << 16) | (FBUF_B(*color) << 8);
            return 4;
        case FBUF_32BPP_RGB888:
            return 4;
        case FBUF_32BPP_RGB888_RE:
            if(color)
                *color = ((*color & 0x000000FF) << 24) | ((*color & 0x0000FF00) << 8) | ((*color & 0x00FF00000) >> 8);
            return 4;
        default:
            return 0;
    }
}

void fbuf_putpixel_stub(void* ptr, size_t x, size_t y, size_t n, uint32_t color, size_t bytes_per_pixel) {
    ptr = (void*) ((uintptr_t) ptr + y * fbuf_impl->pitch + x * bytes_per_pixel);
    uintptr_t line_offset = fbuf_impl->pitch - fbuf_impl->width * bytes_per_pixel;

    /* draw the pixels */
    if(!line_offset && bytes_per_pixel != 3) {
        /* accelerated write using memset16/memset32 */
        switch(bytes_per_pixel) {
            case 2: memset16(ptr, color, n); break;
            case 4: memset32(ptr, color, n); break;
        }
    } else {
        /* (slightly?) slower sequential write */
        for(size_t i = 0; i < n; i++) {
            switch(bytes_per_pixel) {
                case 2:
                    *((uint16_t*) ptr) = color;
                    break;
                case 3:
                    if(fbuf_impl->type == FBUF_24BPP_BGR888) {
                        ((uint8_t*) ptr)[0] = FBUF_R(color);
                        ((uint8_t*) ptr)[1] = FBUF_G(color);
                        ((uint8_t*) ptr)[2] = FBUF_B(color);
                    } else {                    
                        ((uint8_t*) ptr)[0] = FBUF_B(color);
                        ((uint8_t*) ptr)[1] = FBUF_G(color);
                        ((uint8_t*) ptr)[2] = FBUF_R(color);
                    }
                    break;
                case 4:
                    *((uint32_t*) ptr) = color;
                    break;
            }

            ptr = (void*) ((uintptr_t) ptr + bytes_per_pixel);
            x++;
            if(x == fbuf_impl->width) {
                x = 0;
                y++;
                if(y == fbuf_impl->height) return;
                ptr = (void*) ((uintptr_t) ptr + line_offset);
            }
        }
    }
}

void fbuf_putpixel(size_t x, size_t y, size_t n, uint32_t color) {
    if(!fbuf_impl || !n || x >= fbuf_impl->width || y >= fbuf_impl->height) return;

    size_t bytes_per_pixel = fbuf_process_color(&color); // find out number of bytes per pixel and do color data re-shuffling if needed

    if(fbuf_impl->backbuffer)
        fbuf_putpixel_stub(fbuf_impl->backbuffer, x, y, n, color, bytes_per_pixel); // write to backbuffer
    if(fbuf_impl->dbuf_direct_write || !fbuf_impl->backbuffer)
        fbuf_putpixel_stub(fbuf_impl->framebuffer, x, y, n, color, bytes_per_pixel); // write to front buffer if backbuffer is not available or direct writing is enabled
}

void fbuf_getpixel_stub(const fbuf_t* impl, size_t x, size_t y, size_t n, uint32_t* color) {
    if(!impl || !n || x >= impl->width || y >= impl->height) return;

    size_t bytes_per_pixel = fbuf_bytes_per_pixel(impl); // find out number of bytes per pixel

    bool double_buf = (impl->backbuffer);
    void* ptr = (void*) ((uintptr_t)((double_buf) ? impl->backbuffer : impl->framebuffer) + y * impl->pitch + x * bytes_per_pixel); // write to backbuffer if we have one
    uintptr_t line_offset = impl->pitch - impl->width * bytes_per_pixel;
    
    /* draw the pixels */
    uint32_t t;
    for(size_t i = 0; i < n; i++) {
        switch(impl->type) {
            case FBUF_15BPP_RGB555:
                t = *((uint16_t*) ptr);
                *color = (((t & 0b000000000011111) >> 0) << 3) | (((t & 0b000001111100000) >> 5) << 11) | (((t & 0b111110000000000) >> 10) << 19);
                break;
            case FBUF_15BPP_RGB555_RE:
                t = *((uint16_t*) ptr);
                t = ((t & 0x00FF) << 8) | ((t & 0xFF00) >> 8);
                *color = (((t & 0b000000000011111) >> 0) << 3) | (((t & 0b000001111100000) >> 5) << 11) | (((t & 0b111110000000000) >> 10) << 19);
                break;
            case FBUF_15BPP_BGR555:
                t = *((uint16_t*) ptr);
                *color = (((t & 0b111110000000000) >> 10) << 3) | (((t & 0b000001111100000) >> 5) << 11) | (((t & 0b000000000011111) >> 0) << 19);
                break;
            case FBUF_15BPP_BGR555_RE:
                t = *((uint16_t*) ptr);
                t = ((t & 0x00FF) << 8) | ((t & 0xFF00) >> 8);
                *color = (((t & 0b111110000000000) >> 10) << 3) | (((t & 0b000001111100000) >> 5) << 11) | (((t & 0b000000000011111) >> 0) << 19);
                break;
            case FBUF_16BPP_BGR565:
                t = *((uint16_t*) ptr);
                *color = (((t & 0b1111100000000000) >> 11) << 3) | (((t & 0b0000011111100000) >> 5) << 10) | (((t & 0b0000000000011111) >> 0) << 19);
                break;
            case FBUF_16BPP_BGR565_RE:
                t = *((uint16_t*) ptr);
                t = ((t & 0x00FF) << 8) | ((t & 0xFF00) >> 8);
                *color = (((t & 0b1111100000000000) >> 11) << 3) | (((t & 0b0000011111100000) >> 5) << 10) | (((t & 0b0000000000011111) >> 0) << 19);
                break;                        
            case FBUF_16BPP_RGB565:
                t = *((uint16_t*) ptr);
                *color = (((t & 0b0000000000011111) >> 0) << 3) | (((t & 0b0000011111100000) >> 5) << 10) | (((t & 0b1111100000000000) >> 11) << 19);
                break;
            case FBUF_16BPP_RGB565_RE:
                t = *((uint16_t*) ptr);
                t = ((t & 0x00FF) << 8) | ((t & 0xFF00) >> 8);
                *color = (((t & 0b0000000000011111) >> 0) << 3) | (((t & 0b0000011111100000) >> 5) << 10) | (((t & 0b1111100000000000) >> 11) << 19);
                break;
            case FBUF_24BPP_RGB888:
                *color = (((uint8_t*) ptr)[0] << 16) | (((uint8_t*) ptr)[1] << 8) | (((uint8_t*) ptr)[2] << 0);
                break;
            case FBUF_24BPP_BGR888:
                *color = (((uint8_t*) ptr)[2] << 16) | (((uint8_t*) ptr)[1] << 8) | (((uint8_t*) ptr)[0] << 0);
                break;
            case FBUF_32BPP_BGR888:
                t = *((uint32_t*) ptr);
                *color = ((t & 0x000000FF) << 16) | (t & 0x0000FF00) | ((t & 0x00FF0000) >> 16);
                break;
            case FBUF_32BPP_BGR888_RE:
                *color = *((uint32_t*) ptr) >> 8;
                break;
            case FBUF_32BPP_RGB888:
                *color = *((uint32_t*) ptr) & 0x00FFFFFF;
                break;
            case FBUF_32BPP_RGB888_RE:
                t = *((uint32_t*) ptr);
                *color = ((t & 0xFF000000) >> 24) | ((t & 0x00FF0000) >> 8) | ((t & 0x0000FF00) << 8);
                break;
            default: break;
        }

        ptr = (void*) ((uintptr_t) ptr + bytes_per_pixel);
        color = (uint32_t*) ((uintptr_t) color + 4);
        x++;
        if(x == impl->width) {
            x = 0;
            y++;
            if(y == impl->height) return;
            ptr = (void*) ((uintptr_t) ptr + line_offset);
        }
    }
}

void fbuf_getpixel(size_t x, size_t y, size_t n, uint32_t* color) {
    fbuf_getpixel_stub(fbuf_impl, x, y, n, color);
}

void fbuf_fill_stub(void* ptr, size_t y_start, size_t lines, uint32_t color) {
    if(!lines) return;

    size_t bytes_per_pixel = fbuf_process_color(&color); 

    if(bytes_per_pixel == 3) {
        /* no other quicker way than plain putpixel */
        size_t bytes_per_pixel = fbuf_process_color(&color);
        fbuf_putpixel_stub(ptr, 0, y_start, fbuf_impl->width * lines, color, bytes_per_pixel);
    } else {
        ptr = (void*) ((uintptr_t) ptr + y_start * fbuf_impl->pitch); // write to backbuffer if we have one

        if(!(fbuf_impl->pitch % bytes_per_pixel)) { // one single memset for the entire framebuffer
            switch(bytes_per_pixel) {
                case 2: memset16(ptr, color, fbuf_impl->pitch * lines / 2); break;
                case 4: memset32(ptr, color, fbuf_impl->pitch * lines / 4); break;
                default: break;
            }
        } else { // memset per line
            for(size_t y = 0; y < lines; y++) {
                switch(bytes_per_pixel) {
                    case 2: memset16(ptr, color, fbuf_impl->pitch / 2); break;
                    case 4: memset32(ptr, color, fbuf_impl->pitch / 4); break;
                    default: break;
                }
                ptr = (void*) ((uintptr_t) ptr + fbuf_impl->pitch);
            }
        }
    }
}

void fbuf_fill(uint32_t color) {
    if(!fbuf_impl) return;
    if(fbuf_impl->backbuffer)
        fbuf_fill_stub(fbuf_impl->backbuffer, 0, fbuf_impl->height, color);
    if(fbuf_impl->dbuf_direct_write || !fbuf_impl->backbuffer)
        fbuf_fill_stub(fbuf_impl->framebuffer, 0, fbuf_impl->height, color);
}

void fbuf_commit() {
    if(fbuf_impl->dbuf_direct_write || !fbuf_impl->backbuffer) return; // no back buffer or changes have already been committed - don't do anything
    bool intr = intr_test();
    intr_disable();
    if(fbuf_impl->flip) fbuf_impl->flip(fbuf_impl); // use accelerated flip function
    else {        
        size_t fb_size = fbuf_impl->pitch * fbuf_impl->height; // framebuffer size
        size_t pgsz = 0; // VMM page size - we'll set it according to the page in question later
        uintptr_t backbuf_ptr = (uintptr_t) fbuf_impl->backbuffer; // pointer into backbuffer
        if(fbuf_impl->flip_all) {
            memcpy(fbuf_impl->framebuffer, fbuf_impl->backbuffer, fb_size);
            for(size_t off = 0; off < fb_size; off += pgsz, backbuf_ptr += pgsz) {
                vmm_set_dirty(vmm_kernel, backbuf_ptr, false); // clear dirty bit for all pages
                pgsz = vmm_get_pgsz(vmm_kernel, backbuf_ptr);
                kassert(pgsz != (size_t)-1);
                pgsz = vmm_pgsz(pgsz); // resolve pgsz index
            }
            fbuf_impl->flip_all = false;
        } else {
            for(size_t off = 0; off < fb_size; off += pgsz, backbuf_ptr += pgsz) {
                if(vmm_get_dirty(vmm_kernel, backbuf_ptr)) {
                    /* dirty (written) page - this needs to be copied over */
                    memcpy((void*) ((uintptr_t) fbuf_impl->framebuffer + off), (void*)backbuf_ptr, (fb_size - off < pgsz) ? (fb_size - off) : pgsz);
                    vmm_set_dirty(vmm_kernel, backbuf_ptr, false); // clear dirty bit as we've updated the buffer here
                    pgsz = vmm_get_pgsz(vmm_kernel, backbuf_ptr);
                    kassert(pgsz != (size_t)-1);
                    pgsz = vmm_pgsz(pgsz); // resolve pgsz index
                }
            }
        }
    }
    if(intr) intr_enable(); // re-enable interrupts
    fbuf_impl->tick_flip = timer_tick;
}

fbuf_font_t* fbuf_font = NULL;

void fbuf_putc_stub(size_t x, size_t y, char c, uint32_t fg, uint32_t bg, bool transparent) {
    if(!fbuf_font || !fbuf_font->draw || x >= fbuf_impl->width || y >= fbuf_impl->height) return;
    fbuf_font->draw(fbuf_font, x, y, c, fg, bg, transparent);
}

void fbuf_putc(size_t x, size_t y, char c, uint32_t fg, uint32_t bg, bool transparent) {
    if(!fbuf_font || !fbuf_font->draw || x >= fbuf_impl->width || y >= fbuf_impl->height) return;
    fbuf_font->draw(fbuf_font, x, y, c, fg, bg, transparent);
    fbuf_commit();
}

void fbuf_puts_stub(size_t x, size_t y, char* s, uint32_t fg, uint32_t bg, bool transparent) {
    if(!fbuf_font || !fbuf_font->draw || x >= fbuf_impl->width || y >= fbuf_impl->height) return;
    while(*s != '\0') {
        fbuf_font->draw(fbuf_font, x, y, *s, fg, bg, transparent);
        x += fbuf_font->width;
        if(x >= fbuf_impl->width) {
            x = 0;
            y += fbuf_font->height;
            if(y >= fbuf_impl->height) return;
        }
        s++;
    }
}

void fbuf_puts(size_t x, size_t y, char* s, uint32_t fg, uint32_t bg, bool transparent) {
    if(!fbuf_font || !fbuf_font->draw || x >= fbuf_impl->width || y >= fbuf_impl->height) return;
    fbuf_puts_stub(x, y, s, fg, bg, transparent);
    fbuf_commit();
}

static void fbuf_scroll_up_stub(void* ptr_dst, void* ptr_src, size_t lines, uint32_t color) {
    memmove((void*) ptr_dst, (void*) ((uintptr_t) ptr_src + lines * fbuf_impl->pitch), (fbuf_impl->height - lines) * fbuf_impl->pitch);
    fbuf_fill_stub(ptr_dst, fbuf_impl->height - lines, lines, color);
}

void fbuf_scroll_up(size_t lines, uint32_t color) {
    if(!fbuf_impl || !lines) return;
    if(lines > fbuf_impl->height) lines = fbuf_impl->height;

    if(fbuf_impl->scroll_up) {
        fbuf_impl->scroll_up(fbuf_impl, lines);
        fbuf_fill_stub((fbuf_impl->backbuffer) ? fbuf_impl->backbuffer : fbuf_impl->framebuffer, fbuf_impl->height - lines, lines, color);
    } else {
        if(fbuf_impl->dbuf_direct_write || !fbuf_impl->backbuffer)
            fbuf_scroll_up_stub(fbuf_impl->framebuffer, fbuf_impl->backbuffer, lines, color);
        if(fbuf_impl->backbuffer) {
            fbuf_scroll_up_stub(fbuf_impl->backbuffer, fbuf_impl->backbuffer, lines, color);
            fbuf_impl->flip_all = true; // since we've modified the entire framebuffer
        }
    }
}

static void fbuf_scroll_down_stub(void* ptr_dst, void* ptr_src, size_t lines, uint32_t color) {
    memmove((void*) ((uintptr_t) ptr_dst + lines * fbuf_impl->pitch), ptr_src, (fbuf_impl->height - lines) * fbuf_impl->pitch);
    fbuf_fill_stub(ptr_dst, 0, lines, color);
}

void fbuf_scroll_down(size_t lines, uint32_t color) {
    if(!fbuf_impl || !lines) return;
    if(lines > fbuf_impl->height) lines = fbuf_impl->height;

    if(fbuf_impl->scroll_down) {
        fbuf_impl->scroll_down(fbuf_impl, lines);
        fbuf_fill_stub((fbuf_impl->backbuffer) ? fbuf_impl->backbuffer : fbuf_impl->framebuffer, 0, lines, color);
    } else {
        if(fbuf_impl->dbuf_direct_write || !fbuf_impl->backbuffer)
            fbuf_scroll_down_stub(fbuf_impl->framebuffer, fbuf_impl->backbuffer, lines, color);
        if(fbuf_impl->backbuffer) {
            fbuf_scroll_down_stub(fbuf_impl->backbuffer, fbuf_impl->backbuffer, lines, color);
            fbuf_impl->flip_all = true; // since we've modified the entire framebuffer
        }
    }
}

void fbuf_unload() {
    if(!fbuf_impl) return; // nothing to unload

    fbuf_t* fbuf_impl_old = fbuf_impl;
    fbuf_impl = NULL; // say no more

    bool unload_seg = true;
    if(fbuf_impl_old->unload) unload_seg = fbuf_impl_old->unload(fbuf_impl_old); // do pre-unload tasks
    if(unload_seg && fbuf_impl_old->elf_segments) elf_unload_prg(vmm_kernel, fbuf_impl_old->elf_segments, fbuf_impl_old->num_elf_segments); // unload kernel module from memory
}
