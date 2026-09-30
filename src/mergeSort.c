/* src/mergeSort.c — 병합 정렬 */
#include "sortctx.h"

static void mergeRange(SortCtx *c, char *buf, size_t lo, size_t mid, size_t hi) {
    size_t i = lo, j = mid, k = lo;
    const size_t sz = c->size;

    while (i < mid && j < hi) {
        /* '<=' : 같으면 왼쪽을 먼저 — 이 한 글자가 안정성을 만든다 */
        if (sortCmp(c, sortElemAt(c, i), sortElemAt(c, j)) <= 0)
            sortMove(c, buf + (k++) * sz, sortElemAt(c, i++));
        else
            sortMove(c, buf + (k++) * sz, sortElemAt(c, j++));
    }
    sortMoveN(c, buf + k * sz, sortElemAt(c, i), mid - i);
    k += mid - i;
    sortMoveN(c, buf + k * sz, sortElemAt(c, j), hi - j);

    /* 보조 배열에서 원래 자리로 되돌린다 */
    sortMoveN(c, sortElemAt(c, lo), buf + lo * sz, hi - lo);
}

static void mergeSortRec(SortCtx *c, char *buf, size_t lo, size_t hi) {
    if (hi - lo < 2) return;
    sortStackPush(c);
    size_t mid = lo + (hi - lo) / 2;
    mergeSortRec(c, buf, lo, mid);
    mergeSortRec(c, buf, mid, hi);
    mergeRange(c, buf, lo, mid, hi);
    sortStackPop(c);
}

void mergeSort(void *base, size_t n, size_t size, SortCompare cmp, SortStats *stats) {
    SortCtx c;
    if (n < 2) return;
    if (!sortCtxOpen(&c, base, size, cmp, stats)) return;
    char *buf = sortAlloc(&c, n * size);
    if (buf) {
        mergeSortRec(&c, buf, 0, n);
        sortFree(&c, buf, n * size);
    }
    sortCtxClose(&c);
}
