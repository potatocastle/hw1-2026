#!/usr/bin/env python3
"""report/ 아래에 비교 그래프(SVG)를 만든다. 표준 모듈만 쓴다 (matplotlib 없음).

    make charts           # main.out --csv 를 돌려 새로 재고 그린다
    python3 tools/plot.py report/results.csv   # 저장된 측정값으로 다시 그린다
"""
import csv
import io
import math
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "report")
ALGOS = ["mergeSort", "quickSort", "timSort"]
COLORS = {"mergeSort": "#4C78A8", "quickSort": "#F58518", "timSort": "#54A24B"}
SHAPES = [("random", "무작위"), ("sorted", "정렬됨"), ("reversed", "역순"),
          ("dups", "중복많음"), ("nearly", "거의정렬")]
FONT = "'Noto Sans CJK KR','Noto Sans KR','Malgun Gothic','Apple SD Gothic Neo',sans-serif"
W, H = 720, 380
ML, MR, MT, MB = 78, 20, 48, 58


def esc(s):
    return str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def fmt(v):
    if v >= 1e6:
        return f"{v / 1e6:g}M"
    if v >= 1e3:
        return f"{v / 1e3:g}k"
    return f"{v:g}"


class Svg:
    def __init__(self, title):
        self.parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
                      f'width="{W}" height="{H}" font-family="{FONT}">',
                      f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
                      f'<text x="{W / 2}" y="26" text-anchor="middle" font-size="16" '
                      f'font-weight="bold" fill="#222">{esc(title)}</text>']

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=11, anchor="middle", color="#444", extra=""):
        self.add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" text-anchor="{anchor}" '
                 f'fill="{color}" {extra}>{esc(s)}</text>')

    def legend(self, names):
        x = ML + 8
        for n in names:
            self.add(f'<rect x="{x}" y="{MT - 6}" width="12" height="12" fill="{COLORS.get(n, "#999")}"/>')
            self.text(x + 16, MT + 4, n, 11, "start", "#333")
            x += 110

    def save(self, name):
        self.add("</svg>")
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write("\n".join(self.parts))
        print("wrote report/" + name)


class Axis:
    """선형 또는 로그 축 하나"""

    def __init__(self, lo, hi, log, p0, p1, exact=False):
        self.log, self.p0, self.p1 = log, p0, p1
        if log and exact:        # x축: 자료 범위 그대로
            self.lo, self.hi = math.log10(lo), math.log10(hi)
        elif log:                # y축: 10의 거듭제곱 눈금에 맞춘다
            self.lo, self.hi = math.floor(math.log10(lo) + 1e-9), math.ceil(math.log10(hi) - 1e-9)
        else:
            step = self.niceStep(hi / 5)
            self.lo, self.hi, self.step = 0, math.ceil(hi / step) * step, step

    @staticmethod
    def niceStep(raw):
        e = 10 ** math.floor(math.log10(raw))
        for m in (1, 2, 2.5, 5, 10):
            if raw <= m * e:
                return m * e
        return 10 * e

    def __call__(self, v):
        t = ((math.log10(v) if self.log else v) - self.lo) / (self.hi - self.lo)
        return self.p0 + t * (self.p1 - self.p0)

    def ticks(self):
        if self.log:
            return [10 ** k for k in range(self.lo, self.hi + 1)]
        n = int(round(self.hi / self.step))
        return [i * self.step for i in range(n + 1)]


def yGrid(svg, ax, label):
    for t in ax.ticks():
        y = ax(t)
        svg.add(f'<line x1="{ML}" y1="{y:.1f}" x2="{W - MR}" y2="{y:.1f}" stroke="#e5e5e5"/>')
        svg.text(ML - 6, y + 4, fmt(t), 10, "end")
    svg.text(16, (MT + H - MB) / 2, label, 11, "middle", "#444",
             f'transform="rotate(-90 16 {(MT + H - MB) / 2})"')
    svg.add(f'<line x1="{ML}" y1="{H - MB}" x2="{W - MR}" y2="{H - MB}" stroke="#888"/>')


def barChart(name, title, groups, series, ylabel, log=False):
    """groups: [라벨], series: {알고리즘: [값...]}"""
    svg = Svg(title + (" (로그 축)" if log else ""))
    vals = [v for s in series.values() for v in s if v > 0]
    ax = Axis(min(vals) if log else 0, max(vals), log, H - MB, MT + 18)
    yGrid(svg, ax, ylabel)
    gw = (W - ML - MR) / len(groups)
    bw = gw * 0.8 / len(series)
    for gi, g in enumerate(groups):
        svg.text(ML + gw * (gi + 0.5), H - MB + 18, g, 11)
        for si, (algo, s) in enumerate(series.items()):
            v = s[gi]
            if v <= 0:
                continue
            x = ML + gw * gi + gw * 0.1 + bw * si
            base = H - MB
            y = ax(v)
            svg.add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bw - 2:.1f}" height="{max(base - y, 0.5):.1f}" '
                    f'fill="{COLORS[algo]}"/>')
    svg.legend(series.keys())
    svg.save(name)


def lineChart(name, title, xs, series, xlabel, ylabel, logx=True, logy=True, extra=None, ymin=None):
    svg = Svg(title)
    vals = [v for s in series.values() for v in s] + ([v for v in extra[1]] if extra else [])
    lo = ymin if ymin is not None else min(vals)
    ay = Axis(lo, max(vals), logy, H - MB, MT + 18)
    ax = Axis(min(xs), max(xs), logx, ML + 30, W - MR - 30, exact=True)
    if not logy:
        ay.lo = ymin if ymin is not None else 0
        ay.hi = math.ceil(max(vals) / ay.step) * ay.step
    yGrid(svg, ay, ylabel)
    for x in xs:
        svg.text(ax(x), H - MB + 18, f"{x:,}", 10)
    svg.text((ML + W - MR) / 2, H - 12, xlabel, 11)
    if extra:
        pts = " ".join(f"{ax(x):.1f},{ay(y):.1f}" for x, y in zip(xs, extra[1]))
        svg.add(f'<polyline points="{pts}" fill="none" stroke="#999" stroke-dasharray="5,4" stroke-width="1.5"/>')
        mid = len(xs) // 2
        svg.text(ax(xs[mid]) + 6, ay(extra[1][mid]) + 16, extra[0], 10, "start", "#777")
    for algo, s in series.items():
        pts = " ".join(f"{ax(x):.1f},{ay(y):.1f}" for x, y in zip(xs, s))
        svg.add(f'<polyline points="{pts}" fill="none" stroke="{COLORS[algo]}" stroke-width="2.2"/>')
        for x, y in zip(xs, s):
            svg.add(f'<circle cx="{ax(x):.1f}" cy="{ay(y):.1f}" r="3.2" fill="{COLORS[algo]}"/>')
    svg.legend(series.keys())
    svg.save(name)


def slope(xs, ys):
    lx = [math.log(x) for x in xs]
    ly = [math.log(y) for y in ys]
    mx, my = sum(lx) / len(lx), sum(ly) / len(ly)
    return sum((a - mx) * (b - my) for a, b in zip(lx, ly)) / sum((a - mx) ** 2 for a in lx)


def main():
    os.makedirs(OUT, exist_ok=True)
    if len(sys.argv) > 1:
        text = open(sys.argv[1], encoding="utf-8").read()
    else:
        exe = os.path.join(ROOT, "src", "main.out")
        text = subprocess.run([exe, "--csv"], capture_output=True, text=True, check=True).stdout
        with open(os.path.join(OUT, "results.csv"), "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote report/results.csv")
    rows = list(csv.DictReader(io.StringIO(text)))

    def pick(exp, key, **cond):
        out = {}
        for a in ALGOS:
            out[a] = [float(r[key]) for r in rows
                      if r["experiment"] == exp and r["algo"] == a
                      and all(r[k] == v for k, v in cond.items())]
        return out

    # 실험 1: 입력 모양별
    labels = [s[1] for s in SHAPES]
    def byShape(key):
        return {a: [float(next(r[key] for r in rows if r["experiment"] == "shapes"
                               and r["algo"] == a and r["shape"] == s)) for s, _ in SHAPES] for a in ALGOS}
    barChart("shapes-time.svg", "입력 모양별 걸린 시간 (n = 100,000)", labels, byShape("time_ms"), "시간 (ms)")
    barChart("shapes-compares.svg", "입력 모양별 비교 횟수 (n = 100,000)", labels, byShape("compares"), "비교 횟수")
    barChart("shapes-moves.svg", "입력 모양별 이동 횟수 (n = 100,000)", labels, byShape("moves"), "이동 횟수")
    barChart("shapes-moves-log.svg", "입력 모양별 이동 횟수 (n = 100,000)", labels, byShape("moves"), "이동 횟수", log=True)
    barChart("shapes-memory.svg", "입력 모양별 추가 메모리 (n = 100,000)", labels, byShape("extra_bytes"), "바이트", log=True)

    # 실험 2: n을 키우며
    ns = sorted({int(r["n"]) for r in rows if r["experiment"] == "growth"})
    comp = pick("growth", "compares")
    norm = {a: [c / (n * math.log2(n)) for c, n in zip(v, ns)] for a, v in comp.items()}
    lineChart("growth-normalized.svg", "비교 횟수 ÷ n·log₂n (무작위)", ns, norm, "n (로그 축)",
              "비교 / n log n", logx=True, logy=False, ymin=0)
    t = pick("growth", "time_ms")
    lineChart("growth-time-log.svg", "n이 커질 때 걸린 시간 (로그-로그)", ns, t, "n", "시간 (ms)")

    # 실험 3: 런 개수
    rs = sorted({int(r["param"]) for r in rows if r["experiment"] == "runs"})
    n = 100000
    ideal = [n * max(math.log2(r), 0) + n for r in rs]
    lineChart("runs-compares.svg", "오름차순 런 개수에 따른 비교 횟수 (n = 100,000)", rs,
              pick("runs", "compares"), "런 개수 R (로그 축)", "비교 횟수", logx=True, logy=True,
              extra=("n·log₂R + n", ideal))
    lineChart("runs-time.svg", "오름차순 런 개수에 따른 시간 (n = 100,000)", rs,
              pick("runs", "time_ms"), "런 개수 R (로그 축)", "시간 (ms)", logx=True, logy=False)

    print("\n로그-로그 기울기 (비교 횟수, 무작위):")
    for a, v in comp.items():
        print(f"  {a:10s} {slope(ns, v):.3f}")
    print("참고: 같은 구간에서 n log n 의 기울기 = %.3f" % slope(ns, [x * math.log2(x) for x in ns]))


if __name__ == "__main__":
    main()
