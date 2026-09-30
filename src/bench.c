/* src/bench.c — 입력 생성과 측정 */
#define _POSIX_C_SOURCE 199309L
#include "bench.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *SHAPE_NAMES[SHAPE_COUNT] = {
    "무작위", "정렬됨", "역순", "중복많음", "거의정렬", "런묶음",
};
static const char *SHAPE_IDS[SHAPE_COUNT] = {
    "random", "sorted", "reversed", "dups", "nearly", "runs",
};

const char *shapeName(InputShape s) { return (s < SHAPE_COUNT) ? SHAPE_NAMES[s] : "?"; }
const char *shapeId(InputShape s) { return (s < SHAPE_COUNT) ? SHAPE_IDS[s] : "?"; }

/* rand()는 구현마다 분포가 달라 재현성이 없다. 64비트 splitmix를 직접 쓴다. */
static unsigned long long splitmix64(unsigned long long *state) {
    unsigned long long z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static size_t randBelow(unsigned long long *st, size_t bound) {
    return (size_t)(splitmix64(st) % bound);
}

void makeInput(Record *out, size_t n, InputShape shape, size_t param, unsigned long long seed) {
    unsigned long long st = seed;
    for (size_t i = 0; i < n; i++) {
        int key;
        switch (shape) {
        case SHAPE_SORTED:
        case SHAPE_NEARLY:   key = (int)i; break;
        case SHAPE_REVERSED: key = (int)(n - i); break;
        case SHAPE_DUPS:     key = (int)randBelow(&st, 10); break;
        default:             key = (int)randBelow(&st, 1u << 30); break;
        }
        out[i].key = key;
    }
    if (shape == SHAPE_NEARLY) {
        for (size_t s = 0; s < n / 100; s++) {
            size_t a = randBelow(&st, n), b = randBelow(&st, n);
            int t = out[a].key; out[a].key = out[b].key; out[b].key = t;
        }
    }
    if (shape == SHAPE_RUNS && param > 0) {
        /* 무작위 값을 param 조각으로 나누고 조각마다 오름차순으로 만든다 */
        for (size_t r = 0; r < param; r++) {
            size_t lo = n * r / param, hi = n * (r + 1) / param;
            qsort(out + lo, hi - lo, sizeof(Record), recordCompare);
        }
    }
    for (size_t i = 0; i < n; i++) out[i].tag = (int)i;   /* 입력 순서를 새긴다 */
}

int recordCompare(const void *a, const void *b) {
    int x = ((const Record *)a)->key, y = ((const Record *)b)->key;
    return (x > y) - (x < y);    /* tag는 보지 않는다 — 보면 모든 정렬이 안정해 보인다 */
}

int isSortedByKey(const Record *a, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (a[i - 1].key > a[i].key) return 0;
    return 1;
}

int isStableByTag(const Record *a, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (a[i - 1].key == a[i].key && a[i - 1].tag > a[i].tag) return 0;
    return 1;
}

static double nowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

BenchResult benchRun(const SortAlgorithm *algo, const Record *input, size_t n, int reps) {
    BenchResult r;
    memset(&r, 0, sizeof r);
    Record *work = malloc((n ? n : 1) * sizeof(Record));
    if (!work) return r;
    double total = 0;
    if (reps < 1) reps = 1;
    for (int k = 0; k < reps; k++) {
        memcpy(work, input, n * sizeof(Record));
        memset(&r.stats, 0, sizeof r.stats);
        double t0 = nowMs();                       /* 복사는 빼고 잰다 */
        algo->sort(work, n, sizeof(Record), recordCompare, &r.stats);
        total += nowMs() - t0;
    }
    r.ms = total / reps;
    r.sorted = isSortedByKey(work, n);
    r.stable = isStableByTag(work, n);
    free(work);
    return r;
}
