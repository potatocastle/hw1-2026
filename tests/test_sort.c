/* tests/test_sort.c — 유닛 테스트 */
#include "bench.h"
#include "sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks = 0, failures = 0;

static void check(int ok, const char *algo, const char *what) {
    checks++;
    if (!ok) failures++;
    printf("%s  %-10s %s\n", ok ? "ok  " : "FAIL", algo, what);
}

static int intCmp(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* int 배열을 정렬해 qsort 결과와 맞춰 본다 */
static int sortsLikeQsort(const SortAlgorithm *s, const int *in, size_t n) {
    int *a = malloc((n + 1) * sizeof(int)), *b = malloc((n + 1) * sizeof(int));
    memcpy(a, in, n * sizeof(int));
    memcpy(b, in, n * sizeof(int));
    s->sort(a, n, sizeof(int), intCmp, NULL);
    qsort(b, n, sizeof(int), intCmp);
    int ok = memcmp(a, b, n * sizeof(int)) == 0;
    free(a);
    free(b);
    return ok;
}

static void testBasicCases(const SortAlgorithm *s) {
    int shuffled[] = {5, 2, 9, 1, 5, 6, 0, -3, 7};
    int sorted[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int reversed[] = {8, 7, 6, 5, 4, 3, 2, 1};
    int dups[] = {3, 1, 3, 1, 3, 1, 2, 2};
    int same[] = {4, 4, 4, 4, 4};
    int one[] = {42};
    check(sortsLikeQsort(s, shuffled, 9), s->name, "섞인 배열");
    check(sortsLikeQsort(s, sorted, 8), s->name, "이미 정렬된 배열");
    check(sortsLikeQsort(s, reversed, 8), s->name, "역순 배열");
    check(sortsLikeQsort(s, dups, 8), s->name, "중복 값");
    check(sortsLikeQsort(s, same, 5), s->name, "모두 같은 값");
    check(sortsLikeQsort(s, one, 1), s->name, "원소 1개");
    check(sortsLikeQsort(s, one, 0), s->name, "빈 배열");
}

/* n = 0..400 전부. 팀 정렬은 64를 넘어야 병합이 돌고, minrun 경계가 깨지기 쉽다 */
static void testSizeSweep(const SortAlgorithm *s) {
    int ok = 1;
    int *buf = malloc(400 * sizeof(int));
    unsigned int st = 7;
    for (size_t n = 0; n <= 400 && ok; n++) {
        for (int pattern = 0; pattern < 3 && ok; pattern++) {
            for (size_t i = 0; i < n; i++) {
                st = st * 1103515245u + 12345u;
                buf[i] = pattern == 0 ? (int)(st >> 8) % 1000
                       : pattern == 1 ? (int)(st >> 8) % 4
                       : (int)((i % 37) * 3 - (i / 37));   /* 짧은 런이 섞인 톱니 */
            }
            ok = sortsLikeQsort(s, buf, n);
        }
    }
    free(buf);
    check(ok, s->name, "크기 훑기 n = 0..400 (무작위·중복·톱니)");
}

/* 큰 입력: 모든 입력 모양을 Record로 만들어 정렬되는지 */
static void testLargeShapes(const SortAlgorithm *s) {
    const size_t n = 50000;
    Record *in = malloc(n * sizeof(Record));
    int ok = 1;
    for (int sh = 0; sh < SHAPE_COUNT && ok; sh++) {
        makeInput(in, n, (InputShape)sh, 37, 99);
        BenchResult r = benchRun(s, in, n, 1);
        ok = r.sorted;
    }
    free(in);
    check(ok, s->name, "n = 50,000, 입력 모양 6가지 모두 정렬");
}

/* 안정성: 실측이 구현 표의 stable 주장과 일치하는가.
 * - 안정이라고 주장하면: 모든 입력에서 안정해야 한다.
 * - 불안정이라고 주장하면: 불안정함을 보여 주는 입력(증인)이 하나는 있어야 한다. */
static void testStability(const SortAlgorithm *s) {
    const size_t sizes[] = {10, 100, 1000, 20000};
    int allStable = 1;
    Record *in = malloc(20000 * sizeof(Record));
    for (size_t k = 0; k < 4; k++) {
        for (int sh = 0; sh < SHAPE_COUNT; sh++) {
            makeInput(in, sizes[k], (InputShape)sh, 8, 1234 + k);
            if (sh == SHAPE_RANDOM || sh == SHAPE_RUNS)       /* 중복을 만든다 */
                for (size_t i = 0; i < sizes[k]; i++) in[i].key %= 50;
            BenchResult r = benchRun(s, in, sizes[k], 1);
            if (!r.stable) allStable = 0;
        }
    }
    free(in);
    char what[96];
    snprintf(what, sizeof what, "안정성 실측(%s) == 표의 주장(%s)",
             allStable ? "안정" : "불안정", s->stable ? "안정" : "불안정");
    check(allStable == s->stable, s->name, what);
}

/* 측정 도구 자체도 검사한다 */
static void testTools(void) {
    Record a[4] = {{1, 1}, {1, 0}, {2, 2}, {3, 3}};
    check(!isStableByTag(a, 4), "bench", "안정성 판정기가 위반을 잡는다");
    check(isSortedByKey(a, 4), "bench", "정렬 판정기");
    Record b[1000];
    makeInput(b, 1000, SHAPE_SORTED, 0, 1);
    check(isSortedByKey(b, 1000), "bench", "makeInput(정렬됨)은 정렬돼 있다");
    makeInput(b, 1000, SHAPE_REVERSED, 0, 1);
    int strictDesc = 1;
    for (int i = 1; i < 1000; i++) if (b[i - 1].key <= b[i].key) strictDesc = 0;
    check(strictDesc, "bench", "makeInput(역순)은 엄격한 내림차순");
    makeInput(b, 1000, SHAPE_RUNS, 4, 1);
    int descents = 0;
    for (int i = 1; i < 1000; i++) if (b[i - 1].key > b[i].key) descents++;
    check(descents <= 3, "bench", "makeInput(런 4개)은 내려가는 곳이 3곳 이하");
}

/* 측정값 검사: 메모리·스택 주장이 맞는가 */
static void testResourceClaims(void) {
    const size_t n = 100000;
    Record *in = malloc(n * sizeof(Record));
    makeInput(in, n, SHAPE_RANDOM, 0, 5);
    for (size_t a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        const SortAlgorithm *s = &SORT_ALGORITHMS[a];
        BenchResult r = benchRun(s, in, n, 1);
        char what[96];
        if (strcmp(s->name, "quickSort") == 0) {
            snprintf(what, sizeof what, "재귀 깊이 %d <= log2(n)+2 = 18", r.stats.maxStack);
            check(r.stats.maxStack <= 18, s->name, what);
        } else if (strcmp(s->name, "timSort") == 0) {
            snprintf(what, sizeof what, "보조 메모리 %zuB <= n/2 칸 + tmp", r.stats.extraBytes);
            check(r.stats.extraBytes <= (n / 2 + 1) * sizeof(Record), s->name, what);
        } else {
            snprintf(what, sizeof what, "보조 메모리 %zuB >= n 칸", r.stats.extraBytes);
            check(r.stats.extraBytes >= n * sizeof(Record), s->name, what);
        }
    }
    /* 팀 정렬: 정렬된 입력은 비교 n-1번, 이동 0번 */
    makeInput(in, n, SHAPE_SORTED, 0, 5);
    for (size_t a = 0; a < SORT_ALGORITHM_COUNT; a++)
        if (strcmp(SORT_ALGORITHMS[a].name, "timSort") == 0) {
            BenchResult r = benchRun(&SORT_ALGORITHMS[a], in, n, 1);
            check(r.stats.compares == n - 1 && r.stats.moves == 0, "timSort",
                  "정렬된 입력: 비교 n-1, 이동 0");
        }
    free(in);
}

int main(void) {
    for (size_t a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        const SortAlgorithm *s = &SORT_ALGORITHMS[a];
        testBasicCases(s);
        testSizeSweep(s);
        testLargeShapes(s);
        testStability(s);
    }
    testTools();
    testResourceClaims();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
