#include <exec/process.h>
#include <stdlib.h> // for rand()
#include <kernel/log.h>
#include <mm/vmm.h>

/* root process control block - also acts as the kernel process control block */
static struct proc proc_root = {
    NULL,
    NULL,
    NULL,
    NULL
};

struct proc* proc_create(void* vmm, size_t* pid) {
    size_t id = 1; // PID (doesn't really hurt to keep track anyway)
    // the first 1 acts as the guard bit to determine the start of the bit sequence
    struct proc* parent = &proc_root;
    struct proc* proc = NULL;
    while(1) {
        /* traverse from root to find empty slot */
        if(parent != &proc_root && !vmm) { // reuse parent node
            proc = parent;
            break;
        }

        if(!parent->child0) {
            proc = kcalloc(1, sizeof(struct proc));
            if(!proc) {
                kerror("cannot allocate memory for PCB");
                return NULL;
            }
            parent->child0 = proc;
            id = (id << 1) | 0;
            break;
        } else if(!parent->child0->vmm) {
            proc = parent->child0;
            id = (id << 1) | 0;
            break;
        }
        
        if(!parent->child1) {
            proc = kcalloc(1, sizeof(struct proc));
            if(!proc) {
                kerror("cannot allocate memory for PCB");
                return NULL;
            }
            parent->child1 = proc;
            id = (id << 1) | 1;
            break;
        } else if(!parent->child1->vmm) {
            proc = parent->child1;
            id = (id << 1) | 1;
            break;
        }
        int child1 = (rand() & 1);
        parent = child1 ? parent->child1 : parent->child0; // both children exist and are valid - choose a random one
        id = (id << 1) | child1;
    }

    kdebug("created process with PID %u", id);
    if(pid) *pid = id;
    proc->vmm = vmm;
    return proc;
}

void proc_delete(struct proc* proc) {
    proc->vmm = NULL; // invalidate PCB
    
#ifndef PROC_NO_PRUNE // pruning can be disabled e.g. to reduce heap fragmentation
    if(!proc->child0 && !proc->child1) { // end of tree - prune
        struct proc* parent = proc->parent;
        if(parent->child0 == proc) parent->child0 = NULL;
        if(parent->child1 == proc) parent->child1 = NULL;
        kfree(proc);
    }
#endif
}

bool proc_delete_pid(size_t pid) {
    struct proc* proc = proc_get_pcb(pid);
    if(!proc) {
        kerror("cannot find process with PID %u", pid);
        return false;
    }

    proc_delete(proc);
    return true;
}

struct proc* proc_get_pcb(size_t pid) {
    if(!pid) {
        kerror("PID must be non-zero");
        return NULL;
    }

    int bit = sizeof(size_t) * 8 - 1;
    for(; bit >= 0 && !(pid & (1 << bit)); bit--);
    bit--;
    // bit cannot overrun from here as we've validated pid

    struct proc* proc = &proc_root;
    while(bit >= 0 && proc) {
        proc = (pid & (1 << bit--)) ? proc->child1 : proc->child0;
    }

    if(!proc) kerror("cannot find process with PID %u (traversal failed)", pid);
    return proc;
}

size_t proc_get_pid(struct proc* proc) {
    size_t bits = 0, maxbit = sizeof(size_t) * 8 - 2;
    size_t pid = 0;
    for(; bits <= maxbit; bits++) {
        struct proc* parent = proc->parent;
        if(!parent) break; // root reached
        if(parent->child1 == proc) pid |= (1 << (maxbit - bits));
        proc = parent;
    }

    return pid;
}

void proc_init() {
    proc_root.vmm = vmm_kernel;
}