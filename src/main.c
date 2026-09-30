/* src/main.c — 비교 결과 출력
 *
 *   ./src/main.out         사람이 읽는 표
 *   ./src/main.out --csv   같은 측정을 CSV로 (tools/plot.py가 읽는다)
 *
 * 무엇을 잴지는 아래 세 실험 함수에만 적혀 있고, 표로 찍을지 CSV로 찍을지는
 * RowSink 함수 포인터로 갈아 끼운다. 정렬을 갈아 끼운 것과 같은 수다.
 */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEED 20260927ULL

typedef struct Row {
    const char *experiment;   /* shapes | growth | runs */
    InputShape shape;
    size_t n;
    size_t param;             /* runs 실험의 런 개수, 나머지는 0 */
    const SortAlgorithm *algo;
    BenchResult res;
} Row;

typedef void (*RowSink)(const Row *row);

static void tableHeader(const char *title) {
    printf("\n%s\n", title);
    printf("%-10s %-10s %10s %13s %13s %10s %6s %6s %6s\n",
           "입력", "알고리즘", "시간(ms)", "비교", "이동", "메모리", "스택", "정렬", "안정");
    printf("----------------------------------------------------------------------------------------------\n");
}

static void tableRow(const Row *r) {
    char label[32];
    if (strcmp(r->experiment, "runs") == 0) snprintf(label, sizeof label, "런 %zu개", r->param);
    else if (strcmp(r->experiment, "growth") == 0) snprintf(label, sizeof label, "n=%zu", r->n);
    else snprintf(label, sizeof label, "%s", shapeName(r->shape));
    printf("%-12s %-10s %10.3f %13llu %13llu %9zuB %6d %6s %6s\n",
           label, r->algo->name, r->res.ms, r->res.stats.compares, r->res.stats.moves,
           r->res.stats.extraBytes, r->res.stats.maxStack,
           r->res.sorted ? "yes" : "NO", r->res.stable ? "yes" : "no");
}

static void csvRow(const Row *r) {
    printf("%s,%s,%zu,%zu,%s,%.4f,%llu,%llu,%zu,%d,%d,%d\n",
           r->experiment, shapeId(r->shape), r->n, r->param, r->algo->name, r->res.ms,
           r->res.stats.compares, r->res.stats.moves, r->res.stats.extraBytes,
           r->res.stats.maxStack, r->res.sorted, r->res.stable);
}

static void runAll(const char *exp, InputShape shape, size_t n, size_t param, int reps, RowSink sink) {
    Record *input = malloc(n * sizeof(Record));
    if (!input) { fprintf(stderr, "메모리 부족\n"); exit(1); }
    makeInput(input, n, shape, param, SEED + n);
    for (size_t a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        Row row = {.experiment = exp, .shape = shape, .n = n, .param = param, .algo = &SORT_ALGORITHMS[a]};
        row.res = benchRun(&SORT_ALGORITHMS[a], input, n, reps);
        sink(&row);
    }
    free(input);
}

/* 실험 1: 입력 모양별, n = 100,000 */
static void experimentShapes(RowSink sink, int table) {
    const InputShape shapes[] = {SHAPE_RANDOM, SHAPE_SORTED, SHAPE_REVERSED, SHAPE_DUPS, SHAPE_NEARLY};
    if (table) tableHeader("[실험 1] 입력 모양별 (n = 100,000, 5회 평균)");
    for (size_t i = 0; i < sizeof shapes / sizeof shapes[0]; i++)
        runAll("shapes", shapes[i], 100000, 0, 5, sink);
}

/* 실험 2: n을 4배씩 키우며 (무작위) */
static void experimentGrowth(RowSink sink, int table) {
    if (table) tableHeader("[실험 2] n을 키우며 (무작위, 3회 평균)");
    for (size_t n = 1000; n <= 1024000; n *= 4)
        runAll("growth", SHAPE_RANDOM, n, 0, 3, sink);
}

/* 실험 3: 런 개수를 바꿔 가며 (n = 100,000) — 팀 정렬의 적응성 */
static void experimentRuns(RowSink sink, int table) {
    if (table) tableHeader("[실험 3] 오름차순 런 개수를 바꿔 가며 (n = 100,000, 5회 평균)");
    for (size_t r = 1; r <= 16384; r *= 4)
        runAll("runs", SHAPE_RUNS, 100000, r, 5, sink);
}

int main(int argc, char **argv) {
    int csv = (argc > 1 && strcmp(argv[1], "--csv") == 0);
    if (csv) {
        printf("experiment,shape,n,param,algo,time_ms,compares,moves,extra_bytes,max_stack,sorted,stable\n");
        experimentShapes(csvRow, 0);
        experimentGrowth(csvRow, 0);
        experimentRuns(csvRow, 0);
        return 0;
    }
    printf("=== 정렬 비교: 병합 · 퀵 · 팀(Timsort) ===\n");
    printf("원소 = Record{key, tag} 8바이트. 메모리 = 입력 배열 밖에 잡은 최대 바이트.\n");
    printf("스택 = 재귀 깊이(병합·퀵) / 런 스택 최대 높이(팀).\n");
    for (size_t a = 0; a < SORT_ALGORITHM_COUNT; a++) {
        const SortAlgorithm *s = &SORT_ALGORITHMS[a];
        printf("  %-10s 평균 %-11s 최악 %-11s 공간 %-9s %s\n", s->name, s->timeComplexity,
               s->worstComplexity, s->spaceComplexity, s->stable ? "안정" : "불안정");
    }
    experimentShapes(tableRow, 1);
    experimentGrowth(tableRow, 1);
    experimentRuns(tableRow, 1);
    return 0;
}
