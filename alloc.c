#include "cic.h"
static struct mblock *free_blocks;

void InitHeap(Heap hp) {
    hp->head.next = NULL;
    hp->head.begin = hp->head.avail = hp->head.end = NULL;
    hp->last = &hp->head;
}

void* HeapAllocate(Heap hp, unsigned sz) {
    struct mblock *blk = NULL;
    sz = ALIGN(sz, sizeof(union align));
    blk = hp->last;

    while (sz > blk->end - blk->avail) {
        if ((blk->next = free_blocks) != NULL) {
            free_blocks = free_blocks->next;
            blk = blk->next;
        }
        else {
            unsigned m = sz + MBLOCK_SIZE + sizeof(struct mblock);
            blk->next = (struct mblock*)malloc(m);
            blk = blk->next;
            if (!blk)
                Fatal("Memory exhausted");
            blk->end = (char*)blk + m;
        }

        blk->avail = blk->begin = (char*)(blk + 1);
        blk->next = NULL;
        hp->last = blk;
    }

    blk->avail += sz;

    return blk->avail - sz;
}

void FreeHeap(Heap hp) {
    hp->last->next = free_blocks;
    free_blocks = hp->head.next;
    InitHeap(hp);
}
