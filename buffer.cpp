#include "cic.h"

const int INIT_SIZE = 8;

Buffer* makeBuffer() {
    Buffer *r = new Buffer;
    r->body = new char[INIT_SIZE];
    r->nalloc = INIT_SIZE;
    r->len = 0;
    return r;
}

static void reallocBody(Buffer &b) {
    int sz = b.nalloc * 2;
    char *body = new char[sz];
    memcpy(body, b.body, b.len);
    b.body = body;
    b.nalloc = sz;
}

void bufWrite(Buffer &b, char c) {
    if (b.nalloc == b.len + 1) reallocBody(b);
    b.body[b.len++] = c;
}
