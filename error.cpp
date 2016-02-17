#include "cic.h"

bool enable_warning = 1;
bool w_e = 0;

static void printError(const char *line, const char *pos, const char *label, const char *fmt, va_list args) {
    fprintf(stderr, isatty(fileno(stderr)) ? "\e[1;31m[%s]\e[0m " :
            "[%s] ", label);
    fprintf(stderr, "%s: %s: ", line, pos);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void errorf(const char *line, const char *pos, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printError(line, pos, "ERROR", fmt, args);
    va_end(args);
    exit(1);
}

void warnf(const char *line, const char *pos, const char *fmt, ...) {
    if (!enable_warning) return;
    const char *label = w_e ? "ERROR" : "WARN";
    va_list args;
    va_start(args, fmt);
    printError(line, pos, label, fmt, args);
    va_end(args);
    if (w_e) exit(1);
}

const char* tokenPos(Token *tk) {
    File *f = tk->file;
    if (!f) return "(unknown)";
    const char *name = f->name ? f->name :"(unknown)";
    return format("%s:%d:%d", name, tk->line, tk->column);
}
