/* src/quickSort.c -퀵 정렬*/
#include "sortctx.h"
#include <stddef.h>

static void quickSortRec(SortCtx *c, char *pivot, ptrdiff_t lo, ptrdiff_t hi) {
    sortStackPush(c);
    while (lo < hi) {
        ptrdiff_t mid = lo + (hi - lo) / 2;

        /* 세 값의 중앙값: a[lo] <= a[mid] <= a[hi]로 맞춘다 */
        if (sortCmp(c, sortElemAt(c, mid), sortElemAt(c, lo)) < 0)
            sortSwap(c, sortElemAt(c, mid), sortElemAt(c, lo));
        if (sortCmp(c, sortElemAt(c, hi), sortElemAt(c, lo)) < 0)
            sortSwap(c, sortElemAt(c, hi), sortElemAt(c, lo));
        if (sortCmp(c, sortElemAt(c, hi), sortElemAt(c, mid)) < 0)
            sortSwap(c, sortElemAt(c, hi), sortElemAt(c, mid));
        if (hi - lo <= 2) break;          /* 원소 3개 이하는 위에서 이미 정렬됐다 */

        sortMove(c, pivot, sortElemAt(c, mid));

        /* a[lo] <= 피벗 <= a[hi] 이므로 두 스캔은 배열 밖으로 나가지 않는다 */
        ptrdiff_t i = lo, j = hi;
        while (i <= j) {
            while (sortCmp(c, sortElemAt(c, i), pivot) < 0) i++;
            while (sortCmp(c, sortElemAt(c, j), pivot) > 0) j--;
            if (i <= j) {
                sortSwap(c, sortElemAt(c, i), sortElemAt(c, j));
                i++;
                j--;
            }
        }
        /* 이제 [lo..j] <= 피벗 <= [i..hi] */
        if (j - lo < hi - i) {
            quickSortRec(c, pivot, lo, j);  /* 작은 쪽만 재귀 */
            lo = i;
        } else {
            quickSortRec(c, pivot, i, hi);
            hi = j;
        }
    }
    sortStackPop(c);
}

void quickSort(void *base, size_t n, size_t size, SortCompare cmp, SortStats *stats) {
    SortCtx c;
    if (n < 2) return;
    if (!sortCtxOpen(&c, base, size, cmp, stats)) return;
    char *pivot = sortAlloc(&c, size);   /* 피벗 값 한 칸 */
    if (pivot) {
        quickSortRec(&c, pivot, 0, (ptrdiff_t)n - 1);
        sortFree(&c, pivot, size);
    }
    sortCtxClose(&c);
}
