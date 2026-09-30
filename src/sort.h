/* src/sort.h — 정렬 공통 인터페이스 (바깥에 보이는 것은 이것뿐이다) */
#ifndef SORT_H
#define SORT_H

#include <stddef.h>

/* qsort와 같은 규약: 음수 / 0 / 양수 */
typedef int (*SortCompare)(const void *a, const void *b);

/* 정렬이 스스로 세는 값. NULL을 넘기면 측정하지 않는다. */
typedef struct SortStats {
    unsigned long long compares;  /* 비교 함수 호출 횟수 */
    unsigned long long moves;     /* 원소 한 칸 복사 횟수 (교환 = 3) */
    size_t curBytes;              /* 지금 잡고 있는 추가 메모리 */
    size_t extraBytes;            /* 입력 배열 밖에 잡은 최대 바이트 */
    int curStack;                 /* 지금 쌓인 스택 높이 */
    int maxStack;                 /* 재귀 깊이(병합·퀵) 또는 런 스택 높이(팀) */
} SortStats;

typedef struct SortAlgorithm {
    const char *name;
    const char *timeComplexity;   /* 평균 */
    const char *worstComplexity;  /* 최악 */
    const char *spaceComplexity;
    int stable;                   /* 안정 정렬이라고 "주장"하는 값 */
    void (*sort)(void *base, size_t n, size_t size,
                 SortCompare cmp, SortStats *stats);
} SortAlgorithm;

extern const SortAlgorithm SORT_ALGORITHMS[];
extern const size_t SORT_ALGORITHM_COUNT;

void mergeSort(void *base, size_t n, size_t size, SortCompare cmp, SortStats *stats);
void quickSort(void *base, size_t n, size_t size, SortCompare cmp, SortStats *stats);
void timSort(void *base, size_t n, size_t size, SortCompare cmp, SortStats *stats);

#endif
