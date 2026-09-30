#!/usr/bin/env python3
"""팀 정렬 설명용 그림(SVG)을 report/ 아래에 만든다. 표준 모듈만 쓴다."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "report")
FONT = "'Noto Sans CJK KR','Noto Sans KR','Malgun Gothic','Apple SD Gothic Neo',sans-serif"
C = {"asc": "#cfe8cf", "desc": "#f9d9b5", "ins": "#d6e4f5", "ok": "#e8e8e8",
     "hot": "#ffe08a", "edge": "#555"}


class D:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" '
                  f'height="{h}" font-family="{FONT}">', f'<rect width="{w}" height="{h}" fill="#fff"/>',
                  '<defs><marker id="ar" markerWidth="10" markerHeight="8" refX="9" refY="4" orient="auto">'
                  '<path d="M0,0 L10,4 L0,8 z" fill="#555"/></marker></defs>']

    def t(self, x, y, s, size=13, anchor="start", bold=False, color="#222"):
        w = ' font-weight="bold"' if bold else ""
        self.p.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" fill="{color}"{w}>{s}</text>')

    def cells(self, x, y, vals, fills, cw=30, ch=28):
        for i, (v, f) in enumerate(zip(vals, fills)):
            self.p.append(f'<rect x="{x + i * cw}" y="{y}" width="{cw}" height="{ch}" fill="{C[f]}" stroke="{C["edge"]}"/>')
            self.t(x + i * cw + cw / 2, y + ch / 2 + 5, v, 13, "middle")
        return x + len(vals) * cw

    def brace(self, x0, x1, y, label, color="#333"):
        self.p.append(f'<path d="M{x0},{y} v6 H{x1} v-6" fill="none" stroke="{color}"/>')
        self.t((x0 + x1) / 2, y + 22, label, 12, "middle", color=color)

    def arrow(self, x0, y0, x1, y1):
        self.p.append(f'<line x1="{x0}" y1="{y0}" x2="{x1}" y2="{y1}" stroke="#555" stroke-width="1.5" marker-end="url(#ar)"/>')

    def box(self, x, y, w, h, fill, label):
        self.p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="{C[fill]}" stroke="{C["edge"]}"/>')
        self.t(x + w / 2, y + h / 2 + 5, label, 12, "middle")

    def save(self, name):
        self.p.append("</svg>")
        with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
            f.write("\n".join(self.p))
        print("wrote report/" + name)


def runsDiagram():
    d = D(760, 330)
    d.t(20, 28, "① 자연 런 찾기 — 왼쪽부터 이미 정렬된 구간을 찾는다", 14, bold=True)
    vals = ["1", "3", "5", "8", "9", "7", "6", "4", "2", "5", "3", "8"]
    fills = ["asc"] * 5 + ["desc"] * 4 + ["ins"] * 3
    x = d.cells(30, 44, vals, fills)
    d.brace(30, 180, 76, "오름차순 런 (그대로)")
    d.brace(180, 300, 76, "엄격한 내림차순 런 → 뒤집기", "#b35900")
    d.brace(300, 390, 76, "짧은 런", "#2b5d99")

    d.t(20, 142, "② 뒤집기 + minrun까지 늘리기 (이진 삽입 정렬)", 14, bold=True)
    vals2 = ["1", "3", "5", "8", "9", "2", "4", "6", "7", "3", "5", "8"]
    fills2 = ["asc"] * 5 + ["desc"] * 4 + ["ins"] * 3
    d.cells(30, 158, vals2, fills2)
    d.t(410, 172, "실제로는 minrun = 32~64. 짧은 런은 뒤따르는", 12, color="#444")
    d.t(410, 190, "원소를 이진 삽입 정렬로 끌어와 minrun 길이로 만든다.", 12, color="#444")

    d.t(20, 238, "③ 런 스택 — 불변식이 깨지면 이웃 런끼리 병합", 14, bold=True)
    # 스택 그림
    y = 252
    d.box(30, y, 210, 24, "asc", "Z : 길이 120")
    d.box(30, y + 26, 170, 24, "desc", "Y : 길이 70")
    d.box(30, y + 52, 150, 24, "ins", "X : 길이 64 (새 런)")
    d.t(260, y + 18, "불변식  Z > Y + X   그리고   Y > X", 13, bold=True)
    d.t(260, y + 42, "120 > 70 + 64 ?  아니다 → Z가 X보다 길므로 Y와 X를 병합 (134)", 12, color="#444")
    d.t(260, y + 64, "런 길이가 피보나치처럼 자라므로 스택 높이 = O(log n)", 12, color="#2b5d99")
    d.save("timsort-runs.svg")


def gallopDiagram():
    d = D(760, 250)
    d.t(20, 28, "④ 병합 전 갤로핑 — 이미 제자리인 앞뒤를 잘라 내고 가운데만 병합한다", 14, bold=True)
    A = ["1", "2", "3", "7", "9"]
    B = ["4", "5", "8", "10", "11"]
    d.t(30, 66, "A", 14, bold=True)
    d.cells(50, 46, A, ["ok", "ok", "ok", "hot", "hot"], cw=36)
    d.t(260, 66, "B", 14, bold=True)
    d.cells(280, 46, B, ["hot", "hot", "hot", "ok", "ok"], cw=36)
    d.brace(50, 158, 78, "B[0]=4 이하 → 제자리")
    d.brace(388, 460, 78, "A[끝]=9 이상 → 제자리")
    d.t(500, 62, "gallopRight: 1, 3, 7, … 칸씩 뛰며", 12, color="#444")
    d.t(500, 80, "구간을 찾은 뒤 이분 탐색 → O(log k)", 12, color="#444")

    d.t(20, 142, "⑤ 짧은 쪽만 보조 배열로 복사해서 병합 (보조 메모리 ≤ n/2)", 14, bold=True)
    d.cells(50, 158, ["7", "9"], ["hot", "hot"], cw=36)
    d.t(50, 206, "buf ← A의 남은 2칸", 12, color="#444")
    d.arrow(130, 172, 190, 172)
    d.cells(200, 158, ["1", "2", "3", "4", "5", "7", "8", "9", "10", "11"],
            ["ok", "ok", "ok", "hot", "hot", "hot", "hot", "hot", "ok", "ok"], cw=36)
    d.t(200, 206, "mergeLo: 앞에서부터 채운다. 같은 값이면 A를 먼저 → 안정", 12, color="#444")
    d.t(200, 224, "(A가 더 길면 B를 빼고 뒤에서부터 채우는 mergeHi)", 12, color="#444")
    d.save("timsort-gallop.svg")


if __name__ == "__main__":
    runsDiagram()
    gallopDiagram()
