/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2008 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* Keeping it separate allows to NOT pull in stdio for VERY small applets.
 * Try building busybox with only "true" enabled... */

#include "glibbb.h"
#include <stdlib.h>
uint8_t xfunc_error_retval = EXIT_FAILURE;
void (*die_func)(void);

void xfunc_die(void)
{
    if (die_func)
    {
        die_func();
    }
    exit(xfunc_error_retval);
}
