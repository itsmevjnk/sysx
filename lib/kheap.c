#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdlib.h>
#include <kernel/log.h>
#include <stdbool.h>
#include <string.h>

/* magic numbers */
#define KHEAP_HEADER_MAGIC              0xC001BEEF
#define KHEAP_FOOTER_MAGIC              0xDEADBEEF

/* kernel heap block header */
typedef struct {
    uint32_t magic; // magic number - defined above
    size_t size; // block size
    size_t used; // set to non-zero value if in use
} __attribute__((packed)) kheap_header_t;

/* kernel heap block footer */
typedef struct {
    uint32_t magic; // magic number - defined above
    kheap_header_t* header; // the block's header address
} __attribute__((packed)) kheap_footer_t;

#ifndef KHEAP_BASE_ADDRESS
#define KHEAP_BASE_ADDRESS                          0xF0000000 // base address for the kernel heap
#endif

#ifndef KHEAP_MAX_SIZE
#define KHEAP_MAX_SIZE                              16777216 // kernel heap's maximum size in bytes
#endif

#ifndef KHEAP_BLOCK_MIN_SIZE
#define KHEAP_BLOCK_MIN_SIZE                        4 // minimum size of a memory block (to prevent fragmentation)
#endif

#define KHEAP_BLOCK_MIN_SIZE_TOTAL                  (sizeof(kheap_header_t) + KHEAP_BLOCK_MIN_SIZE + sizeof(kheap_footer_t)) // minimum block size including header and footer

kheap_header_t* kheap_first_block = NULL; // ptr to first heap block
size_t kheap_size = 0; // total memory size of the kernel heap

/* helper to retrieve block's footer address */
static inline kheap_footer_t* kheap_footer(kheap_header_t* header) {
    return (kheap_footer_t*) ((uintptr_t) header + sizeof(kheap_header_t) + header->size);
}

/* helper to retrieve last block's header */
static inline kheap_header_t* kheap_last_block() {
    return ((kheap_footer_t*)((uintptr_t) kheap_first_block + kheap_size - sizeof(kheap_footer_t)))->header;
}

/* merge blocks if possible */
static kheap_header_t* kheap_merge(kheap_header_t* header) {
    /* resolve block footer */
    kheap_footer_t* footer = kheap_footer(header);
    // kassert(footer->magic == KHEAP_FOOTER_MAGIC);

    if((uintptr_t) header > (uintptr_t) kheap_first_block) {
        /* check if it can be merged with the previous block */
        kheap_footer_t* prev_footer = (kheap_footer_t*) ((uintptr_t) header - sizeof(kheap_footer_t));
        // kassert(prev_footer->magic == KHEAP_FOOTER_MAGIC);
        // kassert(prev_footer->header->magic == KHEAP_HEADER_MAGIC); // just to be sure
        
        if(!prev_footer->header->used) {
            /* previous block is not in use - we're ready to go */
            prev_footer->header->size += sizeof(kheap_footer_t) + sizeof(kheap_header_t) + header->size; // the prev block will have one more footer and header in addition to the merged block's size
            footer->header = prev_footer->header; // switch to use this block's footer
            header = prev_footer->header; // we'll continue on the previous block
        }
    }

    kheap_header_t* next_header = (kheap_header_t*) ((uintptr_t) footer + sizeof(kheap_footer_t)); // resolve (possible) next block's header
    if((uintptr_t) next_header < (uintptr_t) kheap_first_block + kheap_size) {
        /* this block exists */
        // kassert(next_header->magic == KHEAP_HEADER_MAGIC);

        if(!next_header->used) {
            /* next block is not in use - we're ready to go */
            footer = kheap_footer(next_header); // resolve next block's footer
            // kassert(footer->magic == KHEAP_FOOTER_MAGIC);
            // kassert((uintptr_t) footer->header == (uintptr_t) next_header); // just to be sure

            header->size += sizeof(kheap_footer_t) + sizeof(kheap_header_t) + next_header->size;
            footer->header = header;
        }
    }

    return header; // return the new (merged) block's header
}

/* expand kernel heap */
static size_t kheap_expand(size_t size) {
    if(kheap_size >= KHEAP_MAX_SIZE) {
        kdebug("kernel heap is full");
        return 0;
    }

    /* ask for more memory from PMM */
    size_t requested_frames = (size + pmm_framesz() - 1) / pmm_framesz(); // number of requested frames
    size_t alloc_frames = 0; // set to the number of frames that we were able to allocate
    for(size_t i = 0; i < requested_frames; i++) {
        size_t frame = pmm_first_free(1); // no need for contiguous memory blocks since we'll be mapping to virtual memory anyway
        if(frame == (size_t)-1) break; // out of memory
        pmm_alloc(frame);

        if(kheap_first_block == NULL) { // heap is being initialized
            kheap_first_block = (kheap_header_t*) KHEAP_BASE_ADDRESS;
            // header = kheap_first_block;
        }

        vmm_pgmap(vmm_current, frame * pmm_framesz(), (uintptr_t) kheap_first_block + kheap_size + i * pmm_framesz(), true, false, true); // map new frame to heap memory space
        alloc_frames++;
    }

    if(alloc_frames > 0) {
        /* at least we've allocated something */
        kheap_header_t* header = (kheap_header_t*) ((uintptr_t) kheap_first_block + kheap_size); // last header
        kheap_size += alloc_frames * pmm_framesz(); // update heap size

        header->magic = KHEAP_HEADER_MAGIC;
        header->size = alloc_frames * pmm_framesz() - sizeof(kheap_header_t) - sizeof(kheap_footer_t);
        header->used = 0; // not in use (yet)

        kheap_footer_t* footer = kheap_footer(header);
        footer->magic = KHEAP_FOOTER_MAGIC;
        footer->header = header;

        header = kheap_merge(header); // see if this new block can be merged
    }
    
    return alloc_frames * pmm_framesz();
}

/* handle block alignment case if needed */
static kheap_header_t* kheap_align_block(kheap_header_t* header, uintptr_t return_addr) {
    uintptr_t start_addr = (uintptr_t) header + sizeof(kheap_header_t);

    if(return_addr > start_addr) {
        /* there's unused space before our allocated block - make it into a new block, or cede it to the preceding block */
        size_t orig_size = header->size; // original block size
        if(return_addr - start_addr >= KHEAP_BLOCK_MIN_SIZE_TOTAL) {
            /* make a new block */
            kheap_header_t* prev_header = header;
            prev_header->size = return_addr - start_addr - sizeof(kheap_footer_t) - sizeof(kheap_header_t); // we need to reserve space to add a new footer and header
            kheap_footer_t* prev_footer = kheap_footer(header);
            prev_footer->magic = KHEAP_FOOTER_MAGIC;
            prev_footer->header = prev_header;

            header = (kheap_header_t*) (return_addr - sizeof(kheap_header_t)); // new header
            header->magic = KHEAP_HEADER_MAGIC;
            header->size = orig_size - (return_addr - start_addr);
            header->used = 1; // so kheap_merge won't merge the previous block with this one

            kheap_merge(prev_header); // merge previous header if possible
        } else {
            /* cede space to preceding block */
            // kassert((uintptr_t) header > (uintptr_t) kheap_first_block); // just to be sure here

            kheap_footer_t* prev_footer = (kheap_footer_t*) ((uintptr_t) header - sizeof(kheap_footer_t)); // previous block's footer
            // kassert(prev_footer->magic == KHEAP_FOOTER_MAGIC);

            kheap_header_t* prev_header = prev_footer->header; // previous block's header
            // kassert(prev_header->magic == KHEAP_HEADER_MAGIC); // just to be sure
            prev_header->size += return_addr - start_addr; // increase the previous block's size to cover the unused space

            prev_footer = kheap_footer(prev_header); // get new footer location
            prev_footer->magic = KHEAP_FOOTER_MAGIC;
            prev_footer->header = prev_header;

            header = (kheap_header_t*) (return_addr - sizeof(kheap_header_t)); // new header
            header->magic = KHEAP_HEADER_MAGIC;
            header->size = orig_size - (return_addr - start_addr);
        }

        // kassert(kheap_footer(header)->magic == KHEAP_FOOTER_MAGIC); // in case we got the size wrong
    }

    // kassert(header->size >= size); // make sure nothing went wrong when we cut the preceding space off

    return header;
}

/* cut off extra space from a block if possible */
static void kheap_truncate_block(kheap_header_t* header, size_t size) {
    if(header->size - size >= KHEAP_BLOCK_MIN_SIZE_TOTAL) {
        /* we can cut the unused space into a new block */
        kheap_footer_t* next_footer = kheap_footer(header); // next block's footer (AKA the current block's footer)
        // kassert(next_footer->magic == KHEAP_FOOTER_MAGIC);

        header->size = size; // shrink our block

        kheap_footer_t* footer = kheap_footer(header); // current block's footer
        footer->magic = KHEAP_FOOTER_MAGIC;
        footer->header = header;

        kheap_header_t* next_header = (kheap_header_t*) ((uintptr_t) footer + sizeof(kheap_footer_t)); // next block's header
        next_header->magic = KHEAP_HEADER_MAGIC;
        next_header->size = (size_t) next_footer - ((size_t) next_header + sizeof(kheap_header_t));
        next_header->used = 0;
        next_footer->header = next_header;

        // we don't need to run kheap_merge on this since we assume the original block has already been merged
    }
}

void* kmalloc_ext(size_t size, size_t align, void** phys) {
    if(size == 0) {
        /* do not allocate anything */
        if(phys != NULL) *phys = NULL;
        return NULL;
    }

    if(size < KHEAP_BLOCK_MIN_SIZE) size = KHEAP_BLOCK_MIN_SIZE;

    kheap_header_t* header = kheap_first_block; // our current block
    while(1) {
        /* keep running until we've got a block (or until we can't do so) */
        if((uintptr_t) header - (uintptr_t) kheap_first_block >= kheap_size) {
            /* try to expand the heap */
            size_t requested_size = size + ((align > 1) ? (align - 1) : 0) + sizeof(kheap_header_t) + sizeof(kheap_footer_t);
            size_t expanded_size = kheap_expand(requested_size);
            if(expanded_size < requested_size) {
                /* out of memory */
                kerror("cannot expand heap due to insufficient memory (requested %u, got %u)", requested_size, expanded_size);
                break; // return NULL
            }

            header = kheap_last_block(); // get the actual last block
        }

        /* traverse blocks until we've found one that satisfies our requirement */
        // kassert(header->magic == KHEAP_HEADER_MAGIC); // just to be sure
        if(!header->used) {
            /* this header is not in use */
            bool suitable = false; // set if the block is suitable in terms of memory space

            uintptr_t start_addr = (uintptr_t) header + sizeof(kheap_header_t); // starting address of block data
            uintptr_t return_addr = start_addr; // the address to return to the caller

            if(align > 1) {
                /* alignment requirement */
                if(return_addr % align > 0) return_addr += align - (return_addr % align); // align the returning address
                
                if((uintptr_t) header == (uintptr_t) kheap_first_block) {
                    /* we're on our first block, so we cannot cede the preceding space to any previous blocks */
                    while(return_addr - start_addr < KHEAP_BLOCK_MIN_SIZE_TOTAL) return_addr += align; // trying other aligned addresses until there's enough space to make a new block
                }

                if(return_addr + size <= start_addr + header->size) suitable = true; // data can fit in this block
            } else {
                /* no alignment required */
                if(header->size >= size) suitable = true;
            }

            if(suitable) {
                /* we're all clear and ready to go */
                header = kheap_align_block(header, return_addr);
                kheap_truncate_block(header, size);
                
                header->used = 1; // mark block as used
                if(phys != NULL) *phys = (void*) vmm_physaddr(vmm_current, return_addr);
                return (void*) return_addr;
            }
        }

        header = (kheap_header_t*) ((uintptr_t) header + sizeof(kheap_header_t) + header->size + sizeof(kheap_footer_t)); // next block
    }

    return NULL; // cannot allocate
}

void kfree(void* ptr) {
    if(ptr == NULL) return; // no need to free anything

    kheap_header_t* header = (kheap_header_t*) ((uintptr_t) ptr - sizeof(kheap_header_t));
    if(header->magic != KHEAP_HEADER_MAGIC) kwarn("invalid memory block @ 0x%x", (uintptr_t) ptr);
    header->used = 0;
    kheap_merge(header); // defragment our freed header
}

void* krealloc_ext(void* ptr, size_t size, size_t align, void** phys) {
    if(ptr == NULL) return kmalloc_ext(size, align, phys); // redirect to kmalloc_ext

    if(size == 0) {
        /* redirect to kfree */
        if(phys != NULL) *phys = NULL;
        kfree(ptr);
        return NULL;
    }

    kheap_header_t* header = (kheap_header_t*) ((uintptr_t) ptr - sizeof(kheap_header_t)); // current block's header
    // kassert(header->magic == KHEAP_HEADER_MAGIC);
    size_t move_size = (header->size < size) ? header->size : size; // size of data to be relocated
    
    /* APPROACH 1: EXTEND CURRENT BLOCK */

    /* check previous block if it's available */
    kheap_footer_t* prev_footer = (kheap_footer_t*) ((uintptr_t) header - sizeof(kheap_footer_t)); // previous block's footer (if one exists)
    bool prev_unused = false, prev_block_available = ((uintptr_t) prev_footer >= (uintptr_t) kheap_first_block);
    if(prev_block_available) {
        // kassert(prev_footer->magic == KHEAP_FOOTER_MAGIC);
        // kassert(prev_footer->header->magic == KHEAP_HEADER_MAGIC);
        if(!prev_footer->header->used) prev_unused = true;
    }

    /* check next block if it's available */
    kheap_header_t* next_header = (kheap_header_t*) ((uintptr_t) ptr + header->size + sizeof(kheap_footer_t)); // next block's header (if one exists)
    bool next_unused = false, next_block_available = ((uintptr_t) next_header < (uintptr_t) kheap_first_block + kheap_size);
    if(next_block_available) {
        // kassert(next_header->magic == KHEAP_HEADER_MAGIC);
        if(!next_header->used) next_unused = true;
    }

    /* calculate "projected" block (i.e. current block + adjacent block(s) if available) */
    kheap_header_t* proj_header = header;
    size_t proj_size = header->size;
    if(prev_unused) {
        /* expand to previous block */
        proj_header = prev_footer->header;
        proj_size += prev_footer->header->size + sizeof(kheap_footer_t) + sizeof(kheap_header_t);
    }
    if(next_unused) {
        /* expand to next block */
        proj_size += next_header->size + sizeof(kheap_footer_t) + sizeof(kheap_header_t);
    }
    uintptr_t start_addr = (uintptr_t) proj_header + sizeof(kheap_header_t);
    uintptr_t end_addr = start_addr + proj_size; // end address of projected block (i.e. footer address)
    uintptr_t return_addr = start_addr;
    if(align > 1 && return_addr % align != 0) {
        return_addr += align - (return_addr % align);
        if((uintptr_t) proj_header == (uintptr_t) kheap_first_block) {
            while(return_addr - start_addr < KHEAP_BLOCK_MIN_SIZE_TOTAL) return_addr += align;
        }
    }

    /* extend the next block if needed */
    if((next_unused || !next_block_available) && return_addr + size > end_addr && (uintptr_t) kheap_footer(next_header) + sizeof(kheap_footer_t) == (uintptr_t) kheap_last_block()) {
        size_t orig_size = proj_size; // save projected size
        if(next_unused) proj_size -= next_header->size; // remove next header
        size_t expand_size = return_addr + size - end_addr;
        if(kheap_expand(expand_size) > 0) {
            /* expansion is (somewhat) successful */
            proj_size += next_header->size;
            if(!next_block_available) { // next block is now available, so we also have a new header to absorb into the projected block
                next_block_available = true;
                proj_size += sizeof(kheap_header_t);
            }
            end_addr = start_addr + proj_size; // update end address
        } else proj_size = orig_size; // restore projected size on failure
    }

    if(return_addr + size <= end_addr) {
        /* we can fit the new data block in, and if we need to do alignment, we can either cede space to the previous block or create a new block */

        kheap_merge(header); // merge adjacent blocks (note that this is NOT supposed to affect the block's data). this will also take proj_header to the preceding block if it's merged.
        // kassert((uintptr_t) kheap_merge(header) == (uintptr_t) proj_header); // check if projected header is correct
        // kassert(proj_size == proj_header->size); // just to be sure that our projected size is correct

        memmove((void*) return_addr, ptr, move_size); // relocate block data

        proj_header = kheap_align_block(proj_header, return_addr);
        kheap_truncate_block(proj_header, size);

        proj_header->used = 1; // mark block as used
        if(phys != NULL) *phys = (void*) vmm_physaddr(vmm_current, return_addr);
        return (void*) return_addr;
    }

    /* APPROACH 2: ALLOCATE NEW BLOCK */
    void* new_ptr = kmalloc_ext(size, align, phys);
    if(new_ptr != NULL) {
        /* successful allocation */
        memmove(new_ptr, ptr, move_size);
        kfree(ptr); // free up old block
        return new_ptr;
    }

    return NULL; // nothing works
}

void kheap_dump() {
    if(kheap_first_block != NULL) {
        kdebug("kernel heap @ 0x%x (size: %u):", (uintptr_t) kheap_first_block, kheap_size);
        /* traverse kernel heap */
        kheap_header_t* header = kheap_first_block;
        while((uintptr_t) header < (uintptr_t) kheap_first_block + kheap_size) {
            kdebug(" - header @ 0x%x (data @ 0x%x): magic 0x%08x, size %u, used %u", (uintptr_t) header, (uintptr_t) header + sizeof(kheap_header_t), header->magic, header->size, header->used);
            kheap_footer_t* footer = kheap_footer(header);
            kdebug("   footer @ 0x%x: magic 0x%08x, associated w/ header @ 0x%x", (uintptr_t) footer, footer->magic, (uintptr_t) footer->header);
            header = (kheap_header_t*) ((uintptr_t) footer + sizeof(kheap_footer_t));
        }
    } else kdebug("kernel heap is not initialized");
}
