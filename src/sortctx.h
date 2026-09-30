/* src/sortctx.h — 구현끼리만 쓰는 도구. main.c는 이 파일을 보지 않는다. */
#ifndef SORTCTX_H
#define SORTCTX_H

#include "sort.h"
#include <string.h>

typedef struct SortCtx {
    char *base;        /* 배열의 첫 바이트 */
    size_t size;       /* 원소 한 개의 바이트 수 */
    SortCompare cmp;
    SortStats *stats;
    char *tmp;         /* 원소 한 칸 (교환·삽입용) */
} SortCtx;

/* 문맥을 열고 닫는다. tmp 한 칸도 추가 메모리로 센다. 실패하면 0. */
int sortCtxOpen(SortCtx *c, void *base, size_t size, SortCompare cmp, SortStats *stats);
void sortCtxClose(SortCtx *c);

/* 추가 메모리를 잡고 놓는다. 최대치를 stats->extraBytes에 남긴다. */
void *sortAlloc(SortCtx *c, size_t bytes);
void sortFree(SortCtx *c, void *p, size_t bytes);

/* 스택 높이(재귀 깊이 또는 런 스택) 기록 */
void sortStackPush(SortCtx *c);
void sortStackPop(SortCtx *c);
void sortStackSet(SortCtx *c, int height);

static inline char *sortElemAt(const SortCtx *c, size_t i) {
    return c->base + i * c->size;
}

/* 비교 한 번. 반드시 이것을 거쳐야 센다. */
static inline int sortCmp(SortCtx *c, const void *a, const void *b) {
    if (c->stats) c->stats->compares++;
    return c->cmp(a, b);
}

/* 원소 count칸 복사 (겹쳐도 된다). 이동 count회. */
static inline void sortMoveN(SortCtx *c, void *dst, const void *src, size_t count) {
    if (count == 0 || dst == src) return;
    memmove(dst, src, count * c->size);
    if (c->stats) c->stats->moves += count;
}

static inline void sortMove(SortCtx *c, void *dst, const void *src) {
    sortMoveN(c, dst, src, 1);
}

/* tmp를 거치므로 이동 3회 */
static inline void sortSwap(SortCtx *c, void *a, void *b) {
    if (a == b) return;
    sortMove(c, c->tmp, a);
    sortMove(c, a, b);
    sortMove(c, b, c->tmp);
}

#endif
