#include "cic.h"

void Error(Coord cd, const char *format, ...) {
    va_list ap;

    error_count++;
    if (cd)
        fprintf(stderr, "(%s,%d):", cd->filename, cd->ppline);
    fprintf(stderr, "error:");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void Fatal(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    exit(-1);
}

void Warning(Coord cd, const char *format, ...) {
    va_list ap;

    warning_count++;
    if (cd)
        fprintf(stderr, "(%s,%d):", cd->filename, cd->ppline);

    fprintf(stderr, "warning:");
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
