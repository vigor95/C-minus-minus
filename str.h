#ifndef __STR_H_
#define __STR_H_

typedef struct namebucket {
    char *name;
    int len;
    struct namebucket *link;
} *NameBucket;

typedef struct string {
    char *chars;
    int len;
} *String;

#define NAME_HASH_MASK 1023

char* InternName(char*, int);
void AppendStr(String, char*, int, int);

#endif
