#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "input.h"
#include "error.h"

unsigned char END_OF_FILE = 255;
Input input;

void ReadSourceFile(char *filename) {
    int len;
    input.file = fopen(filename, "r");
    if (input.file == NULL)
        Fatal("Cannot open file: %s. ", filename);
    fseek(input.file, 0, SEEK_END);
    input.size = ftell(input.file);
    input.base = (unsigned char*)malloc(input.size + 1);
    fclose(input.file);

    input.filename = filename;
    input.base[input.size] = END_OF_FILE;
    input.cursor = input.line_head = input.base;
    input.line = 1;
}

void CloseSourceFile() {
    free(input.base);
}
