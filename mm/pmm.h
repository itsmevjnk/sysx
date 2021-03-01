#ifndef MM_PMM_H
#define MM_PMM_H

#include <stddef.h>
#include <stdint.h>

/*
 * uintptr_t* pmm_bitmap
 *  Points to the PMM bitmap. This is supposed to be filled
 *  during bootstrap.
 */
extern uintptr_t* pmm_bitmap;

/*
 * size_t pmm_frames
 *  Total number of page frames. This is supposed to be filled
 *  by pmm_init().
 */
extern size_t pmm_frames;

/*
 * size_t pmm_framesz()
 *  Returns the size of each page frame (which is the size of each page)
 */
size_t pmm_framesz();

/*
 * int pmm_alloc(size_t frame)
 *  Marks the specified frame as in use, then returns 0 on success.
 */
int pmm_alloc(size_t frame);

/*
 * void pmm_free(size_t frame)
 *  Marks the specified frame as free.
 */
void pmm_free(size_t frame);

/*
 * size_t pmm_first_free(size_t sz)
 *  Finds the first sz frame(s) of contiguous free frames and return it
 *  if available, otherwise returns -1.
 */
size_t pmm_first_free(size_t sz);

/*
 * void pmm_init()
 *  Initializes the PMM bitmap. This is supposed to be implemented
 *  specific to the target.
 */
void pmm_init();


#endif
