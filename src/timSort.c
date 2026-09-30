/* src/timSort.c — 팀 정렬 */
#include "sortctx.h"
#include <assert.h>
#include <stddef.h>

#define TIM_MIN_MERGE 64      /* 이보다 짧은 배열은 이진 삽입 정렬 한 번으로 끝 */
#define TIM_MAX_PENDING 85    /* 런 스택 최대 높이 (CPython과 같다. 2^64 원소도 충분) */

typedef struct TimState {
    SortCtx *c;
    char *buf;                /* 병합용 보조 배열 (필요할 때만 늘린다) */
    size_t bufCap;            /* 원소 개수 기준 용량 */
    size_t runBase[TIM_MAX_PENDING];
    size_t runLen[TIM_MAX_PENDING];
    int nRuns;
} TimState;

/* n >= 64 이면 32..64 사이의 값. n/minrun이 2의 거듭제곱에 가깝게 만든다. */
size_t timSortMinRun(size_t n) {
    size_t r = 0;
    while (n >= TIM_MIN_MERGE) {
        r |= n & 1;
        n >>= 1;
    }
    return n + r;
}

/* [lo, hi)를 이진 삽입 정렬한다. [lo, start)는 이미 정렬돼 있다. */
static void binaryInsertionSort(SortCtx *c, size_t lo, size_t hi, size_t start) {
    if (start == lo) start++;
    for (; start < hi; start++) {
        sortMove(c, c->tmp, sortElemAt(c, start));
        /* upper bound: 같은 값의 "뒤"에 넣어야 안정하다 */
        size_t left = lo, right = start;
        while (left < right) {
            size_t m = left + (right - left) / 2;
            if (sortCmp(c, c->tmp, sortElemAt(c, m)) < 0) right = m;
            else left = m + 1;
        }
        sortMoveN(c, sortElemAt(c, left + 1), sortElemAt(c, left), start - left);
        sortMove(c, sortElemAt(c, left), c->tmp);
    }
}

static void reverseRange(SortCtx *c, size_t lo, size_t hi) {
    while (hi > lo + 1) {
        hi--;
        sortSwap(c, sortElemAt(c, lo), sortElemAt(c, hi));
        lo++;
    }
}

/* lo에서 시작하는 런의 길이. 내림차순이면 뒤집어서 돌려준다.
 * 내림차순은 "엄격하게"(<) 센다. 같은 값을 포함해 뒤집으면 순서가 바뀌어
 * 안정성이 깨지기 때문이다. */
static size_t countRunAndMakeAscending(SortCtx *c, size_t lo, size_t hi) {
    size_t k = lo + 1;
    if (k == hi) return 1;
    if (sortCmp(c, sortElemAt(c, k), sortElemAt(c, lo)) < 0) {
        k++;
        while (k < hi && sortCmp(c, sortElemAt(c, k), sortElemAt(c, k - 1)) < 0) k++;
        reverseRange(c, lo, k);
    } else {
        k++;
        while (k < hi && sortCmp(c, sortElemAt(c, k), sortElemAt(c, k - 1)) >= 0) k++;
    }
    return k - lo;
}

/* a[0..len)에서 key가 들어갈 가장 오른쪽 자리 (a[0..k) <= key < a[k..)).
 * 앞에서부터 1, 3, 7, 15 ... 칸씩 뛰며(갤로핑) 구간을 찾고 그 안을 이분 탐색. */
static size_t gallopRight(SortCtx *c, const char *key, const char *a, size_t len) {
    const size_t sz = c->size;
    if (len == 0 || sortCmp(c, key, a) < 0) return 0;
    size_t lastOfs = 0, ofs = 1;
    while (ofs < len && sortCmp(c, key, a + ofs * sz) >= 0) {
        lastOfs = ofs;
        ofs = ofs * 2 + 1;
    }
    if (ofs > len) ofs = len;
    size_t lo = lastOfs + 1, hi = ofs;
    while (lo < hi) {
        size_t m = lo + (hi - lo) / 2;
        if (sortCmp(c, key, a + m * sz) >= 0) lo = m + 1;
        else hi = m;
    }
    return lo;
}

/* a[0..len)에서 key가 들어갈 가장 왼쪽 자리 (a[0..k) < key <= a[k..)).
 * 이번에는 끝에서부터 갤로핑한다 — 찾는 자리가 끝 쪽에 있을 가능성이 크다. */
static size_t gallopLeftFromEnd(SortCtx *c, const char *key, const char *a, size_t len) {
    const size_t sz = c->size;
    if (len == 0 || sortCmp(c, a + (len - 1) * sz, key) < 0) return len;
    size_t lastOfs = 0, ofs = 1;
    while (ofs < len && sortCmp(c, a + (len - 1 - ofs) * sz, key) >= 0) {
        lastOfs = ofs;
        ofs = ofs * 2 + 1;
    }
    if (ofs > len) ofs = len;
    size_t lo = len - ofs, hi = len - 1 - lastOfs;
    while (lo < hi) {
        size_t m = lo + (hi - lo) / 2;
        if (sortCmp(c, a + m * sz, key) < 0) lo = m + 1;
        else hi = m;
    }
    return lo;
}

static int ensureBuf(TimState *ts, size_t need) {
    if (need <= ts->bufCap) return 1;
    SortCtx *c = ts->c;
    size_t cap = need;                    /* CPython처럼 필요한 만큼만 (최대 n/2) */
    sortFree(c, ts->buf, ts->bufCap * c->size);
    ts->buf = sortAlloc(c, cap * c->size);
    ts->bufCap = ts->buf ? cap : 0;
    return ts->buf != NULL;
}

/* A가 짧거나 같다: A를 보조 배열로 빼고 앞에서부터 채운다. */
static void mergeLo(TimState *ts, size_t baseA, size_t lenA, size_t baseB, size_t lenB) {
    SortCtx *c = ts->c;
    const size_t sz = c->size;
    if (!ensureBuf(ts, lenA)) return;
    sortMoveN(c, ts->buf, sortElemAt(c, baseA), lenA);

    size_t i = 0, j = baseB, endB = baseB + lenB, k = baseA;
    while (i < lenA && j < endB) {
        /* B가 "엄격히" 작을 때만 B를 먼저 — 같으면 A가 앞 (안정성) */
        if (sortCmp(c, sortElemAt(c, j), ts->buf + i * sz) < 0)
            sortMove(c, sortElemAt(c, k++), sortElemAt(c, j++));
        else
            sortMove(c, sortElemAt(c, k++), ts->buf + (i++) * sz);
    }
    sortMoveN(c, sortElemAt(c, k), ts->buf + i * sz, lenA - i);
    /* B의 나머지는 이미 제자리다 */
}

/* B가 짧다: B를 보조 배열로 빼고 뒤에서부터 채운다. */
static void mergeHi(TimState *ts, size_t baseA, size_t lenA, size_t baseB, size_t lenB) {
    SortCtx *c = ts->c;
    const size_t sz = c->size;
    if (!ensureBuf(ts, lenB)) return;
    sortMoveN(c, ts->buf, sortElemAt(c, baseB), lenB);

    ptrdiff_t i = (ptrdiff_t)(baseA + lenA) - 1;   /* A의 끝 (배열 안) */
    ptrdiff_t j = (ptrdiff_t)lenB - 1;             /* B의 끝 (보조 배열) */
    ptrdiff_t k = (ptrdiff_t)(baseB + lenB) - 1;
    const ptrdiff_t startA = (ptrdiff_t)baseA;
    while (i >= startA && j >= 0) {
        /* B가 엄격히 작을 때만 A를 뒤로 — 같으면 B가 뒤 (안정성) */
        if (sortCmp(c, ts->buf + j * sz, sortElemAt(c, i)) < 0)
            sortMove(c, sortElemAt(c, k--), sortElemAt(c, i--));
        else
            sortMove(c, sortElemAt(c, k--), ts->buf + (j--) * sz);
    }
    if (j >= 0) sortMoveN(c, sortElemAt(c, k - j), ts->buf, (size_t)j + 1);
}

/* 스택의 i번째와 i+1번째 런을 병합한다. */
static void mergeAt(TimState *ts, int i) {
    SortCtx *c = ts->c;
    size_t baseA = ts->runBase[i], lenA = ts->runLen[i];
    size_t baseB = ts->runBase[i + 1], lenB = ts->runLen[i + 1];

    ts->runLen[i] = lenA + lenB;
    if (i == ts->nRuns - 3) {             /* 맨 위 셋 중 아래 둘을 합치면 맨 위를 내린다 */
        ts->runBase[i + 1] = ts->runBase[i + 2];
        ts->runLen[i + 1] = ts->runLen[i + 2];
    }
    ts->nRuns--;
    sortStackSet(c, ts->nRuns);

    /* B[0]보다 작거나 같은 A의 앞부분은 이미 제자리 */
    size_t k = gallopRight(c, sortElemAt(c, baseB), sortElemAt(c, baseA), lenA);
    baseA += k;
    lenA -= k;
    if (lenA == 0) return;
    /* A의 마지막보다 크거나 같은 B의 뒷부분도 이미 제자리 */
    lenB = gallopLeftFromEnd(c, sortElemAt(c, baseA + lenA - 1), sortElemAt(c, baseB), lenB);
    if (lenB == 0) return;

    if (lenA <= lenB) mergeLo(ts, baseA, lenA, baseB, lenB);
    else mergeHi(ts, baseA, lenA, baseB, lenB);
}

/* 스택 불변식 (위에서부터 X, Y, Z, W):
 *   Z > Y + X   그리고   Y > X   (W > Z + Y 도 확인 — 2015년 버그 수정판)
 * 이것이 지켜지면 런 길이가 피보나치 수보다 빨리 자라 스택 높이가 O(log n)이다. */
static void mergeCollapse(TimState *ts) {
    while (ts->nRuns > 1) {
        int n = ts->nRuns - 2;
        size_t *len = ts->runLen;
        if ((n > 0 && len[n - 1] <= len[n] + len[n + 1]) ||
            (n > 1 && len[n - 2] <= len[n - 1] + len[n])) {
            if (len[n - 1] < len[n + 1]) n--;
            mergeAt(ts, n);
        } else if (len[n] <= len[n + 1]) {
            mergeAt(ts, n);
        } else {
            break;
        }
    }
#ifndef NDEBUG
    for (int i = 0; i + 1 < ts->nRuns; i++) {
        assert(ts->runLen[i] > ts->runLen[i + 1]);
        if (i + 2 < ts->nRuns) assert(ts->runLen[i] > ts->runLen[i + 1] + ts->runLen[i + 2]);
    }
#endif
}

static void mergeForceCollapse(TimState *ts) {
    while (ts->nRuns > 1) {
        int n = ts->nRuns - 2;
        if (n > 0 && ts->runLen[n - 1] < ts->runLen[n + 1]) n--;
        mergeAt(ts, n);
    }
}

void timSort(void *base, size_t n, size_t size, SortCompare cmp, SortStats *stats) {
    SortCtx c;
    if (n < 2) return;
    if (!sortCtxOpen(&c, base, size, cmp, stats)) return;

    if (n < TIM_MIN_MERGE) {              /* 작은 배열: 런 하나 + 이진 삽입 */
        size_t run = countRunAndMakeAscending(&c, 0, n);
        binaryInsertionSort(&c, 0, n, run);
        sortCtxClose(&c);
        return;
    }

    TimState ts = {.c = &c, .buf = NULL, .bufCap = 0, .nRuns = 0};
    size_t minRun = timSortMinRun(n);
    size_t lo = 0;
    while (lo < n) {
        size_t run = countRunAndMakeAscending(&c, lo, n);
        if (run < minRun) {
            size_t force = (n - lo < minRun) ? n - lo : minRun;
            binaryInsertionSort(&c, lo, lo + force, lo + run);
            run = force;
        }
        assert(ts.nRuns < TIM_MAX_PENDING);
        ts.runBase[ts.nRuns] = lo;
        ts.runLen[ts.nRuns] = run;
        ts.nRuns++;
        sortStackSet(&c, ts.nRuns);
        mergeCollapse(&ts);
        lo += run;
    }
    mergeForceCollapse(&ts);
    assert(ts.nRuns == 1 && ts.runLen[0] == n);

    sortFree(&c, ts.buf, ts.bufCap * size);
    sortCtxClose(&c);
}
