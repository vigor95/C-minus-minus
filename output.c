#include "cic.h"
#include "output.h"

#define BUF_LEN 4096

#define PUT_CHAR(c)                 \
    if (buffer_size + 1 > BUF_LEN) {\
        Flush();                    \
    }                               \
    out_buffer[buffer_size++] = c;

char out_buffer[BUF_LEN];
int buffer_size;

FILE* CreateOutput(char *filename, char *ext) {
    char tmp[256];
    char *p = tmp;

    while (*filename && *filename != '.')
        *p++ = *filename++;
    strcpy(p, ext);

    return fopen(tmp, "w");
}

char* FormatName(const char *fmt, ...) {
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    return InternName(buf, strlen(buf));
}
