#include "cic.h"

const int INIT_SIZE = 8;

Buffer* makeBuffer() {
    Buffer *r = new Buffer;
    r->body = new char[INIT_SIZE];
    r->nalloc = INIT_SIZE;
    r->len = 0;
    return r;
}

static void reallocBody(Buffer *b) {
    int sz = b->nalloc * 2;
    char *body = new char[sz];
    memcpy(body, b->body, b->len);
    b->body = body;
    b->nalloc = sz;
}

char* bufBody(Buffer *b) {
    return b->body;
}

int bufLen(Buffer *b) {
    return b->len;
}

void bufWrite(Buffer *b, char c) {
    if (b->nalloc == b->len + 1) reallocBody(b);
    b->body[b->len++] = c;
}

void bufAppend(Buffer *b, char *s, int len) {
    for (int i = 0; i < len; i++)
        bufWrite(b, s[i]);
}

void bufPrintf(Buffer *b, const char *fmt, ...) {
    va_list args;
    while (1) {
        int ok = b->nalloc - b->len;
        va_start(args, fmt);
        int written = vsnprintf(b->body + b->len, ok, fmt, args);
        va_end(args);
        if (ok <= written) {
            reallocBody(b);
            continue;
        }
        b->len += written;
        return;
    }
}

char* vformat(const char *fmt, va_list ap) {
    Buffer *b = makeBuffer();
    va_list aq;
    while (1) {
        int ok = b->nalloc - b->len;
        va_copy(aq, ap);
        int written = vsnprintf(b->body + b->len, ok, fmt, aq);
        va_end(aq);
        if (ok <= written) {
            reallocBody(b);
            continue;
        }
        b->len += written;
        return bufBody(b);
    }
}

char* format(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *r = vformat(fmt, ap);
    va_end(ap);
    return r;
}

static const char* quote(char c) {
    switch (c) {
        case '"': return "\\\"";
        case '\\': return "\\\\";
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
    }
    return NULL;
}

static void print(Buffer *b, char c) {
    const char *q = quote(c);
    if (q) bufPrintf(b, "%s", q);
    else if (isprint(c)) bufPrintf(b, "%c", c);
    else bufPrintf(b, "\\x%02x", c);
}

char* quoteCstring(char *p) {
    Buffer *b = makeBuffer();
    while (*p) print(b, *p++);
    return bufBody(b);
}

const char* quoteChar(char c) {
    if (c == '\\') return "\\\\";
    if (c == '\'') return "\\'";
    return format("%c", c);
}
