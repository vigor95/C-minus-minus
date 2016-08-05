#ifndef __INPUT_H_
#define __INPUT_H_

typedef struct coord {
    char *filename;
    int ppline;
    int line;
    int col;
} *Coord;

typedef struct {
    char *filename;
    unsigned char *base;
    unsigned char *cursor;
    unsigned char *line_head;
    int line;
    void *file;
    void *file_mapping;
    unsigned long size;
} Input;

extern unsigned char END_OF_FILE;
extern Input input;

void ReadSourceFile(char *filename);
void CloseSourceFile();

#endif
