#include "cic.h"

std::vector<File> files;

static void closeFile(File &f) {
    if (f.file) fclose(f.file);
}

static int readcFile(File &f) {
    int c = getc(f.file);
    if (c == EOF) {
        c = (f.last == '\n' || f.last == EOF) ? EOF : '\n';       
    }
    else if (c == '\r') {
        int c2 = getc(f.file);
        if (c2 != '\n') ungetc(c2, f.file);
        c = '\n';                            
    }
    f.last = c;
    return c;
}

static int readcString(File &f) {
    int c;
    if (*f.p == '\0') c = (f.last == '\n' || f.last == EOF) ? EOF : '\n';
    else if (*f.p == '\r') {
        f.p++;
        if (*f.p == '\n') f.p++;
        c = '\n';
    }
    else c = *f.p++;
    f.last = c;
    return c;
}

static int get() {
    File f = files[files.size() - 1];
    int c;
    if (f.buflen > 0) c = f.buf[--f.buflen];
    else if (f.file) c = readcFile(f);
    else c = readcString(f);
    if (c == '\n') {
        f.line++;
        f.column = 1;
    }
    else if (c != EOF) f.column++;
    return c;
}

void unreadc(int c) {
    if (c == EOF) return;
    File f = files[files.size() - 1];
    assert(f.buflen < sizeof(f.buf) / sizeof(f.buf[0]));
    f.buf[f.buflen++] = c;
    if (c == '\n') {
        f.column = 1;
        f.line--;
    }
    else f.column--;
}

int readc() {
    while (1) {
        int c = get();
        if (c == EOF) {
            if (files.size() == 1) return c;
            files.pop_back();
            closeFile(files[files.size() - 1]);
            continue;
        }
        if (c != '\\') return c;
        int c2 = get();
        if (c2 == '\n') continue;
        unreadc(c2);
        return c;
    }
}

File* currentFile() {
    return &files[files.size() - 1];
}
