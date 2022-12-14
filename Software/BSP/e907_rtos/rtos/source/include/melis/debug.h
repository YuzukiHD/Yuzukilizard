#ifndef __MELIS_DEBUG_H__
#define __MELIS_DEBUG_H__

#include <arch.h>

#define software_break(...)  \
    do {                     \
        arch_break();        \
    } while(0)

void gdb_start(void);

#endif /*__MELIS_DEBUG_H__*/

