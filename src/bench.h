/* src/bench.h — 시간 · 메모리 · 안정성 측정 */
#ifndef BENCH_H
#define BENCH_H

#include "sort.h"

/* int 배열로는 안정성을 볼 수 없다. 입력 순서를 tag에 새겨 둔다. */
typedef struct Record {
    int key;   /* 정렬 기준 */
    int tag;   /* 입력에서의 순서 */
} Record;

typedef enum InputShape {
    SHAPE_RANDOM,     /* 무작위 (거의 모두 다른 값) */
    SHAPE_SORTED,     /* 이미 정렬됨 */
    SHAPE_REVERSED,   /* 역순 (엄격한 내림차순) */
    SHAPE_DUPS,       /* 값 10종만 — 중복 많음 */
    SHAPE_NEARLY,     /* 정렬 후 n/100 쌍을 무작위로 교환 — 거의 정렬 */
    SHAPE_RUNS,       /* 오름차순 런 param개를 이어 붙임 */
    SHAPE_COUNT
} InputShape;

const char *shapeName(InputShape shape);   /* 사람이 읽는 이름 */
const char *shapeId(InputShape shape);     /* CSV용 영문 이름 */

/* 씨앗이 같으면 어느 기계에서나 같은 입력 (자체 난수 splitmix64 사용) */
void makeInput(Record *out, size_t n, InputShape shape, size_t param, unsigned long long seed);

int recordCompare(const void *a, const void *b);   /* key만 본다 */
int isSortedByKey(const Record *a, size_t n);
int isStableByTag(const Record *a, size_t n);      /* 같은 key끼리 tag 오름차순? */

typedef struct BenchResult {
    double ms;          /* reps회 평균 (입력 복사는 빼고 잰다) */
    SortStats stats;    /* 마지막 한 번의 카운터 (입력이 같으니 매번 같다) */
    int sorted;
    int stable;
} BenchResult;

BenchResult benchRun(const SortAlgorithm *algo, const Record *input, size_t n, int reps);

#endif
