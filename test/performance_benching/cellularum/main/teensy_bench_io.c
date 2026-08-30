#if defined(__IMXRT1062__)

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "usb_serial.h"

int dbench_printf(const char *fmt, ...);

int dbench_printf(const char *fmt, ...)
{
    char line[256];
    va_list args;

    va_start(args, fmt);
    const int wanted = vsnprintf(line, sizeof line, fmt, args);
    va_end(args);

    if (wanted <= 0)
    {
        return wanted;
    }

    const uint32_t sent = ((size_t)wanted < sizeof line) ? (uint32_t)wanted : (uint32_t)(sizeof line - 1u);

    (void)usb_serial_write(line, sent);
    return wanted;
}

void dbench_flush(void);

void dbench_flush(void)
{
    usb_serial_flush_output();
}

#endif
