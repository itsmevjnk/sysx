#ifndef KERNEL_SETTINGS_H
#define KERNEL_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#define KSYM_PATH           "/boot/kernel.sym" // path to kernel symbols file
#define KMOD_PATH           "/boot/modules" // path to kernel modules directory
#define DEVFS_ROOT          "/dev" // path to devices directory

#define KMOD_INIT_FUNC      "kmod_init" // kernel module initialization function name

#endif