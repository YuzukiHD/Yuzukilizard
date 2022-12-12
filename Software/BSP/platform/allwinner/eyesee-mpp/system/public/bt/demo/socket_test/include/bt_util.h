#ifndef _BLUETOOTH_UTIL_H
#define _BLUETOOTH_UTIL_H

#include <hardware/bluetooth.h>


int str2bd(char *str, bt_bdaddr_t *addr);

char *bd2str(const bt_bdaddr_t *bdaddr, char bdstr[]);

void bdt_log(const char *fmt_str, ...);

#endif
