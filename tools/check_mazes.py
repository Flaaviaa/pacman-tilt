"""Confere os labirintos de src/maze.c: 20x18, borda fechada, tudo conectado.

Uso: python tools/check_mazes.py
"""
import re
import sys
from pathlib import Path

W, H = 20, 18
src = (Path(__file__).resolve().parent.parent / "src" / "maze.c").read_text(encoding="utf-8")
ok = True

for name, body in re.findall(r"maze_(\w+)\[MAZE_H\]\s*=\s*\{(.*?)\};", src, re.S):
    rows = re.findall(r'"([^"]*)"', body)
    errs = []
    if len(rows) != H:
        errs.append(f"{len(rows)} linhas (esperado {H})")
    for i, r in enumerate(rows):
        if len(r) != W:
            errs.append(f"linha {i} tem {len(r)} colunas")
    if not errs:
        if any(c != "#" for c in rows[0] + rows[-1]) or any(r[0] != "#" or r[-1] != "#" for r in rows):
            errs.append("borda aberta")
        if sum(r.count("P") for r in rows) != 1:
            errs.append("precisa de exatamente um 'P'")
        free = {(x, y) for y, r in enumerate(rows) for x, c in enumerate(r) if c != "#"}
        start = next(iter(free))
        seen, stack = {start}, [start]
        while stack:
            x, y = stack.pop()
            for n in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if n in free and n not in seen:
                    seen.add(n)
                    stack.append(n)
        if seen != free:
            errs.append(f"{len(free - seen)} celulas inacessiveis: {sorted(free - seen)[:5]}")
    pellets = sum(r.count(".") + r.count("o") for r in rows)
    print(f"maze_{name}: {'OK' if not errs else 'ERRO'} ({pellets} pastilhas, {sum(r.count('G') for r in rows)} celulas G)")
    for e in errs:
        print("   -", e)
    ok &= not errs

sys.exit(0 if ok else 1)
