/* src/sort.c — 공통 도구 + 구현 표 */
#include "sortctx.h"
#include <stdlib.h>

/* 정렬을 하나 더 넣으려면 이 표에 한 줄 넣는다. 테스트·측정이 저절로 따라온다. */
const SortAlgorithm SORT_ALGORITHMS[] = {
    {"mergeSort", "O(n log n)", "O(n log n)", "O(n)",     1, mergeSort},
    {"quickSort", "O(n log n)", "O(n^2)",     "O(log n)", 0, quickSort},
    {"timSort",   "O(n log n)", "O(n log n)", "O(n)",     1, timSort},
};
const size_t SORT_ALGORITHM_COUNT = sizeof SORT_ALGORITHMS / sizeof SORT_ALGORITHMS[0];

int sortCtxOpen(SortCtx *c, void *base, size_t size, SortCompare cmp, SortStats *stats) {
    c->base = base;
    c->size = size;
    c->cmp = cmp;
    c->stats = stats;
    c->tmp = sortAlloc(c, size);
    return c->tmp != NULL;
}

void sortCtxClose(SortCtx *c) {
    sortFree(c, c->tmp, c->size);
    c->tmp = NULL;
}

void *sortAlloc(SortCtx *c, size_t bytes) {
    void *p = malloc(bytes ? bytes : 1);
    if (p && c->stats) {
        c->stats->curBytes += bytes;
        if (c->stats->curBytes > c->stats->extraBytes)
            c->stats->extraBytes = c->stats->curBytes;
    }
    return p;
}

void sortFree(SortCtx *c, void *p, size_t bytes) {
    if (!p) return;
    free(p);
    if (c->stats) c->stats->curBytes -= bytes;
}

void sortStackPush(SortCtx *c) {
    if (!c->stats) return;
    c->stats->curStack++;
    if (c->stats->curStack > c->stats->maxStack) c->stats->maxStack = c->stats->curStack;
}

void sortStackPop(SortCtx *c) {
    if (c->stats) c->stats->curStack--;
}

void sortStackSet(SortCtx *c, int height) {
    if (!c->stats) return;
    c->stats->curStack = height;
    if (height > c->stats->maxStack) c->stats->maxStack = height;
}
