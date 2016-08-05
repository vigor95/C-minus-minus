#ifndef __OUTPUT_H_
#define __OUTPUT_H_

void LeftAlign(FILE*, int);
FILE* CreateOutput(char*, char*);
char* FormatName(const char *, ...);
void Print(const char*);
void PutString(char*);
void PutChar(int);
void Flush();

#endif
