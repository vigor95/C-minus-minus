#ifndef __ALLOC_H_
#define __ALLOC_H_

struct mblock {
    struct mblock *next;
    char *begin;
    char *avail;
    char *end;
};

union align {
    double d;
    int (*f)(void);
};

typedef struct heap {
    struct mblock *last;
    struct mblock head;
} *Heap;

#define ALLOC(p) ((p) = HeapAllocate(current_heap, sizeof *(p)))
#define CALLOC(p) memset(ALLOC(p), 0, sizeof *(p))
#define MBLOCK_SIZE (4 * 1024)
#define HEAP(hp) struct heap hp = { &hp.head }

void InitHeap(Heap hp);
void* HeapAllocate(Heap hp, unsigned sz);
void FreeHeap(Heap hp);

#endif
