#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <stdarg.h>

#include <hardware/hardware.h>
#include <hardware/bluetooth.h>

void bdt_log(const char *fmt_str, ...)
{
    static char buffer[1024];
    va_list ap;

    va_start(ap, fmt_str);
    vsnprintf(buffer, 1024, fmt_str, ap);
    va_end(ap);

    fprintf(stdout, "%s\n", buffer);
}

void get_bdaddr(const char *str, bt_bdaddr_t *bd)
{
    char *d = ((char *)bd), *endp;
    int i;
    for(i = 0; i < 6; i++)
    {
        *d++ = strtol(str, &endp, 16);
        if (*endp != ':' && i != 5)
        {
            memset(bd, 0, sizeof(bt_bdaddr_t));
            return;
        }
        str = endp + 1;
    }
}

int bdaddr_compare(const bt_bdaddr_t *a, const bt_bdaddr_t *b)
{
    const uint8_t *addra = a->address;
    const uint8_t *addrb = b->address;

    if ((addra == NULL) || (addrb == NULL))
    {
        fprintf(stdout, "%s() arg is NULL.", __FUNCTION__);
        return 1;
    }

    if ((addra[0] == addrb[0]) && (addra[1] == addrb[1]) && (addra[2] == addrb[2]) && (addra[3] == addrb[3]) && (addra[4] == addrb[4]) && (addra[5] == addrb[5]))
    {
        return 0;
    }

    return 1;
}

int str2bd(char *str, bt_bdaddr_t *addr)
{
    int32_t i = 0;
    for (i = 0; i < 6; i++)
    {
       addr->address[i] = (uint8_t)strtoul(str, &str, 16);
       str++;
    }
    return 0;
}

char *bd2str(const bt_bdaddr_t *bdaddr, char bdstr[])
{
    const uint8_t *addr = bdaddr->address;

    sprintf(bdstr, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2],
             addr[3], addr[4], addr[5]);
    return bdstr;
}
