"""Gera o projeto KiCad 8 da placa do Pac-Tilt (face simples, trilhas de 0,3 mm).

Uso: python hardware/gen_kicad.py
Saida: hardware/pacman-tilt.kicad_pro, .kicad_sch, .kicad_pcb

Layout (vista de cima, componentes THT no topo, cobre so na face de baixo B.Cu):
  - display ST7789 a esquerda (barra de pinos na borda direita do modulo)
  - BlackPill no centro, USB voltado para BAIXO
  - MPU6050 em cima a direita, botoes 12 mm embaixo a direita
Coordenadas em mm, origem no canto superior esquerdo da placa.
"""
import json
import uuid
from pathlib import Path

OUT = Path(__file__).resolve().parent
NAME = "pacman-tilt"

BOARD_W, BOARD_H = 120.0, 70.0
TRACK = 0.3
CLEARANCE = 0.25
P = 2.54

NETS = ["", "GND", "+3V3", "LCD_SCK", "LCD_SDA", "LCD_RES", "LCD_DC", "LCD_BLK",
        "I2C_SCL", "I2C_SDA", "BTN_A", "BTN_B"]
NET = {n: i for i, n in enumerate(NETS)}


def uid():
    return str(uuid.uuid4())


ROOT_UUID = uid()

# ---------------------------------------------------------------------------
# Componentes: pinos (numero, nome, rede, x, y) em coordenadas absolutas da placa
# ---------------------------------------------------------------------------
BP_L, BP_R, BP_Y0 = 56.76, 72.0, 10.0
BP_LEFT = ["VB", "C13", "C14", "C15", "R", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
           "B0", "B1", "B2", "B10", "3V3", "G", "5V"]
BP_RIGHT = ["3V3", "G", "5V", "B9", "B8", "B7", "B6", "B5", "B4", "B3", "A15", "A12", "A11",
            "A10", "A9", "A8", "B15", "B14", "B13", "B12"]
BP_NETS = {
    ("L", "A5"): "LCD_SCK", ("L", "A7"): "LCD_SDA", ("L", "B0"): "LCD_RES", ("L", "B1"): "LCD_DC",
    ("L", "B10"): "LCD_BLK", ("L", "3V3"): "+3V3", ("L", "G"): "GND",
    ("R", "3V3"): "+3V3", ("R", "G"): "GND", ("R", "B7"): "I2C_SDA", ("R", "B6"): "I2C_SCL",
    ("R", "B13"): "BTN_B", ("R", "B12"): "BTN_A",
}

DISP_X, DISP_Y0 = 40.0, 30.32
DISP_PINS = [("GND", "GND"), ("VCC", "+3V3"), ("SCK", "LCD_SCK"), ("SDA", "LCD_SDA"),
             ("RES", "LCD_RES"), ("DC", "LCD_DC"), ("BLK", "LCD_BLK")]

MPU_X, MPU_Y0 = 85.0, 10.0
MPU_PINS = [("VCC", "+3V3"), ("GND", "GND"), ("SCL", "I2C_SCL"), ("SDA", "I2C_SDA"),
            ("XDA", ""), ("XCL", ""), ("AD0", ""), ("INT", "")]

# Botao tatil 12x12 mm: furos 12,5 x 5,0 mm. Usa pinos em diagonal (1 e 4),
# que sempre ficam em lados diferentes da chave.
SW = {"SW1": ((88.0, 61.0), "BTN_B", "B (pausa)"), "SW2": ((104.0, 61.0), "BTN_A", "A (confirma)")}


def sw_pads(cx, cy):
    return {"1": (cx - 6.25, cy - 2.5), "2": (cx + 6.25, cy - 2.5),
            "3": (cx - 6.25, cy + 2.5), "4": (cx + 6.25, cy + 2.5)}


comps = {}   # ref -> dict(sym_uuid, pads={num: (name, net, x, y, size, drill)})

pads = {}
for i, n in enumerate(BP_LEFT):
    pads[str(i + 1)] = (n, BP_NETS.get(("L", n), ""), BP_L, BP_Y0 + i * P, 1.7, 1.0)
for i, n in enumerate(BP_RIGHT):
    pads[str(i + 21)] = (n, BP_NETS.get(("R", n), ""), BP_R, BP_Y0 + i * P, 1.7, 1.0)
comps["U1"] = dict(value="BlackPill_STM32F411", fp="BlackPill_F411_USB_Down", pads=pads)

comps["J1"] = dict(value="ST7789_240x240", fp="Display_ST7789_1x07", pads={
    str(i + 1): (n, net, DISP_X, DISP_Y0 + i * P, 1.7, 1.0) for i, (n, net) in enumerate(DISP_PINS)})
comps["J2"] = dict(value="MPU6050_GY521", fp="MPU6050_GY521_1x08", pads={
    str(i + 1): (n, net, MPU_X, MPU_Y0 + i * P, 1.7, 1.0) for i, (n, net) in enumerate(MPU_PINS)})
for ref, ((cx, cy), net, _) in SW.items():
    sp = sw_pads(cx, cy)
    comps[ref] = dict(value="SW_Push_12mm", fp="SW_Tactile_12x12mm", pads={
        "1": ("1", net, *sp["1"], 2.2, 1.3), "2": ("2", "", *sp["2"], 2.2, 1.3),
        "3": ("3", "", *sp["3"], 2.2, 1.3), "4": ("4", "GND", *sp["4"], 2.2, 1.3)})
for c in comps.values():
    c["sym_uuid"] = uid()

# ---------------------------------------------------------------------------
# Trilhas (todas em B.Cu, 0,3 mm). Cada entrada: rede, lista de pontos.
# ---------------------------------------------------------------------------
y = lambda i: BP_Y0 + i * P
TRACKS = [
    # Display -> lado esquerdo da BlackPill (mesma ordem, sem cruzamentos)
    ("LCD_SCK", [(40, 35.40), (BP_L, y(10))]),
    ("LCD_SDA", [(40, 37.94), (50, 37.94), (52.54, 40.48), (BP_L, y(12))]),
    ("LCD_RES", [(40, 40.48), (48, 40.48), (50.54, 43.02), (BP_L, y(13))]),
    ("LCD_DC",  [(40, 43.02), (46, 43.02), (48.54, 45.56), (BP_L, y(14))]),
    ("LCD_BLK", [(40, 45.56), (44, 45.56), (49.08, 50.64), (BP_L, y(16))]),
    # Alimentacao do display: contorna os pinos por fora e chega no 3V3/G de baixo
    ("+3V3", [(40, 32.86), (36.5, 32.86), (36.5, y(17)), (BP_L, y(17))]),
    ("GND",  [(40, 30.32), (34, 30.32), (34, y(18)), (BP_L, y(18))]),
    # GND dos botoes: sai do G esquerdo, desce pelo vao sob a BlackPill
    ("GND",  [(BP_L, y(18)), (60, y(18)), (60, 66.5), (110.25, 66.5), (110.25, 63.5)]),
    ("GND",  [(94.25, 66.5), (94.25, 63.5)]),
    # MPU6050 -> lado direito (alto) da BlackPill
    ("+3V3",    [(BP_R, y(0)), (MPU_X, 10.0)]),
    ("GND",     [(BP_R, y(1)), (MPU_X, 12.54)]),
    ("I2C_SDA", [(BP_R, y(5)), (76, y(5)), (81.08, 17.62), (MPU_X, 17.62)]),
    # SCL passa por baixo da barra do MPU e entra no pino pela direita
    ("I2C_SCL", [(BP_R, y(6)), (77, y(6)), (82.76, 31.0), (88, 31.0), (88, 15.08), (MPU_X, 15.08)]),
    # Botoes
    ("BTN_B", [(BP_R, y(18)), (78.5, y(18)), (81.28, 58.5), (81.75, 58.5)]),
    ("BTN_A", [(BP_R, y(19)), (75, y(19)), (77.74, 61.0), (96, 61.0), (96, 58.5), (97.75, 58.5)]),
]

HOLES = [(4, 4), (116, 4), (4, 66), (116, 66)]

# Contornos dos modulos (silk) - medidas aproximadas, confira com os seus modulos
OUTLINES = {
    "U1": (54.38, 6.63, 74.38, 61.63),     # BlackPill 20 x 55 mm (medido)
    "J1": (3.0, 24.44, 42.0, 51.44),       # display 39 x 27 mm, barra na borda de 27 mm (medido)
    "J2": (83.7, 8.89, 98.7, 28.89),       # GY-521 15 x 20 mm
    "SW1": (82.0, 55.0, 94.0, 67.0),
    "SW2": (98.0, 55.0, 110.0, 67.0),
}

# ---------------------------------------------------------------------------
# Verificacao de folgas (DRC simples)
# ---------------------------------------------------------------------------
def seg_dist(a, b, c, d):
    def pt_seg(p, s0, s1):
        (px, py), (x0, y0), (x1, y1) = p, s0, s1
        dx, dy = x1 - x0, y1 - y0
        L = dx * dx + dy * dy
        t = 0 if L == 0 else max(0, min(1, ((px - x0) * dx + (py - y0) * dy) / L))
        return ((px - x0 - t * dx) ** 2 + (py - y0 - t * dy) ** 2) ** 0.5

    def inter(p1, p2, p3, p4):
        def o(a, b, c):
            return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        return (o(p1, p2, p3) * o(p1, p2, p4) < 0) and (o(p3, p4, p1) * o(p3, p4, p2) < 0)

    if inter(a, b, c, d):
        return 0.0
    return min(pt_seg(a, c, d), pt_seg(b, c, d), pt_seg(c, a, b), pt_seg(d, a, b))


def check():
    segs = [(net, p[i], p[i + 1]) for net, p in TRACKS for i in range(len(p) - 1)]
    allpads = [(ref, num, net, x, yy, size) for ref, c in comps.items()
               for num, (_, net, x, yy, size, _) in c["pads"].items()]
    errs, worst = [], 99.0
    for i, (n1, a, b) in enumerate(segs):
        for n2, c, d in segs[i + 1:]:
            if n1 == n2:
                continue
            g = seg_dist(a, b, c, d) - TRACK
            worst = min(worst, g)
            if g < CLEARANCE:
                errs.append(f"trilhas {n1} x {n2}: folga {g:.2f} mm em {a}-{b} / {c}-{d}")
        for ref, num, pn, x, yy, size in allpads:
            if pn == n1:
                continue
            g = seg_dist(a, b, (x, yy), (x, yy)) - TRACK / 2 - size / 2
            worst = min(worst, g)
            if g < CLEARANCE:
                errs.append(f"trilha {n1} x pad {ref}.{num} ({pn or 'sem rede'}): folga {g:.2f} mm")
        for hx, hy in HOLES:
            if seg_dist(a, b, (hx, hy), (hx, hy)) - TRACK / 2 - 1.6 < 0.5:
                errs.append(f"trilha {n1} muito perto do furo {hx},{hy}")
        for p in (a, b):
            if min(p[0], p[1], BOARD_W - p[0], BOARD_H - p[1]) < 1.0:
                errs.append(f"trilha {n1} perto da borda em {p}")
    # Cada trilha deve comecar e terminar em pad (ou em outra trilha) da mesma rede
    endpoints = {}
    for net, p in TRACKS:
        for q in (p[0], p[-1]):
            ok = any(net == pn and abs(q[0] - x) < 1e-6 and abs(q[1] - yy) < 1e-6
                     for _, _, pn, x, yy, _ in allpads)
            ok = ok or any(net == n2 and seg_dist(q, q, s0, s1) < 1e-6
                           for n2, s0, s1 in segs if not (s0 == q or s1 == q))
            if not ok:
                errs.append(f"ponta solta da rede {net} em {q}")
    # Todos os pads de uma rede devem estar ligados (conectividade)
    parent = {}

    def find(k):
        parent.setdefault(k, k)
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    def union(a, b):
        parent[find(a)] = find(b)

    pts = lambda q: (round(q[0], 3), round(q[1], 3))
    for net, p in TRACKS:
        for i in range(len(p) - 1):
            union(pts(p[i]), pts(p[i + 1]))
    for net, s0, s1 in segs:
        for net2, q0, q1 in segs:
            if net == net2:
                for q in (q0, q1):
                    if seg_dist(q, q, s0, s1) < 1e-6:
                        union(pts(q), pts(s0))
    for ref, num, pn, x, yy, _ in allpads:
        union(("pad", ref, num), pts((x, yy)))
    # A BlackPill liga internamente 3V3 com 3V3 e G com G
    union(("pad", "U1", "18"), ("pad", "U1", "21"))
    union(("pad", "U1", "19"), ("pad", "U1", "22"))
    for net in NETS[1:]:
        roots = {find(("pad", ref, num)) for ref, num, pn, *_ in allpads if pn == net}
        if len(roots) > 1:
            errs.append(f"rede {net} nao esta totalmente ligada ({len(roots)} partes)")
    return errs, worst


# ---------------------------------------------------------------------------
# Escrita do .kicad_pcb
# ---------------------------------------------------------------------------
def fnum(v):
    return f"{v:.4f}".rstrip("0").rstrip(".")


def footprint(ref, c):
    pts = list(c["pads"].values())
    ox, oy = pts[0][2], pts[0][3]          # origem = pad 1
    x0, y0, x1, y1 = OUTLINES[ref]
    out = [f'  (footprint "pactilt:{c["fp"]}" (layer "F.Cu") (uuid "{uid()}") (at {fnum(ox)} {fnum(oy)})']
    out.append(f'    (property "Reference" "{ref}" (at {fnum(x0 - ox + 1.5)} {fnum(y0 - oy - 1.2)} 0) (layer "F.SilkS") (uuid "{uid()}") (effects (font (size 1 1) (thickness 0.15))))')
    out.append(f'    (property "Value" "{c["value"]}" (at {fnum((x0 + x1) / 2 - ox)} {fnum(y1 - oy + 1.2)} 0) (layer "F.Fab") (uuid "{uid()}") (effects (font (size 1 1) (thickness 0.15))))')
    out.append(f'    (property "Footprint" "pactilt:{c["fp"]}" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness 0.15))))')
    out.append(f'    (path "/{c["sym_uuid"]}")')
    out.append('    (attr through_hole)')
    for layer, w in (("F.SilkS", 0.12), ("F.CrtYd", 0.05)):
        m = 0 if layer == "F.SilkS" else 0.25
        out.append(f'    (fp_rect (start {fnum(x0 - ox - m)} {fnum(y0 - oy - m)}) (end {fnum(x1 - ox + m)} {fnum(y1 - oy + m)}) (stroke (width {w}) (type solid)) (fill none) (layer "{layer}") (uuid "{uid()}"))')
    for num, (name, net, x, yy, size, drill) in c["pads"].items():
        netpart = f' (net {NET[net]} "{net}")' if net else ""
        shape = "rect" if num == "1" and ref in ("U1", "J1", "J2") else "circle"
        out.append(f'    (pad "{num}" thru_hole {shape} (at {fnum(x - ox)} {fnum(yy - oy)}) (size {size} {size}) (drill {drill}) (layers "*.Cu" "*.Mask"){netpart} (pinfunction "{name}") (pintype "passive") (uuid "{uid()}"))')
        if ref in ("U1", "J1", "J2"):
            side = -1 if (ref == "U1" and x == BP_L) or ref == "J1" else 1
            if ref == "J1":
                side = 1     # nomes do display ficam entre o display e a BlackPill
            if ref == "U1":
                side = 1 if x == BP_L else -1   # nomes da BlackPill ficam por dentro
            if ref == "J2":
                side = 1
            tx = x - ox + side * 2.4
            just = "left" if side > 0 else "right"
            out.append(f'    (fp_text user "{name}" (at {fnum(tx)} {fnum(yy - oy)} 0) (layer "F.SilkS") (uuid "{uid()}") (effects (font (size 0.8 0.8) (thickness 0.12)) (justify {just})))')
    out.append("  )")
    return "\n".join(out)


def hole(x, yy):
    return (f'  (footprint "pactilt:MountingHole_3.2mm_M3" (layer "F.Cu") (uuid "{uid()}") (at {fnum(x)} {fnum(yy)})\n'
            f'    (property "Reference" "H" (at 0 -3 0) (layer "F.SilkS") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness 0.15))))\n'
            f'    (property "Value" "M3" (at 0 3 0) (layer "F.Fab") (hide yes) (uuid "{uid()}") (effects (font (size 1 1) (thickness 0.15))))\n'
            f'    (attr board_only exclude_from_pos_files exclude_from_bom)\n'
            f'    (fp_circle (center 0 0) (end 3 0) (stroke (width 0.12) (type solid)) (fill none) (layer "F.SilkS") (uuid "{uid()}"))\n'
            f'    (pad "" np_thru_hole circle (at 0 0) (size 3.2 3.2) (drill 3.2) (layers "*.Cu" "*.Mask") (uuid "{uid()}"))\n'
            f'  )')


def write_pcb():
    L = ['(kicad_pcb (version 20240108) (generator "pactilt_gen") (generator_version "8.0")',
         '  (general (thickness 1.6) (legacy_teardrops no))',
         '  (paper "A4")',
         '  (title_block (title "Pac-Tilt - placa face simples") (company "ELF74 Sistemas Embarcados - UTFPR") (comment 1 "Cobre somente em B.Cu, trilhas 0,3 mm"))',
         '  (layers',
         '    (0 "F.Cu" signal) (31 "B.Cu" signal)',
         '    (32 "B.Adhes" user "B.Adhesive") (33 "F.Adhes" user "F.Adhesive")',
         '    (34 "B.Paste" user) (35 "F.Paste" user)',
         '    (36 "B.SilkS" user "B.Silkscreen") (37 "F.SilkS" user "F.Silkscreen")',
         '    (38 "B.Mask" user) (39 "F.Mask" user)',
         '    (40 "Dwgs.User" user "User.Drawings") (41 "Cmts.User" user "User.Comments")',
         '    (42 "Eco1.User" user "User.Eco1") (43 "Eco2.User" user "User.Eco2")',
         '    (44 "Edge.Cuts" user) (45 "Margin" user)',
         '    (46 "B.CrtYd" user "B.Courtyard") (47 "F.CrtYd" user "F.Courtyard")',
         '    (48 "B.Fab" user) (49 "F.Fab" user)',
         '  )',
         '  (setup (pad_to_mask_clearance 0) (allow_soldermask_bridges_in_footprints no))']
    for i, n in enumerate(NETS):
        L.append(f'  (net {i} "{n}")')
    for ref, c in comps.items():
        L.append(footprint(ref, c))
    for hx, hy in HOLES:
        L.append(hole(hx, hy))
    for net, p in TRACKS:
        for a, b in zip(p, p[1:]):
            L.append(f'  (segment (start {fnum(a[0])} {fnum(a[1])}) (end {fnum(b[0])} {fnum(b[1])}) (width {TRACK}) (layer "B.Cu") (net {NET[net]}) (uuid "{uid()}"))')
    L.append(f'  (gr_rect (start 0 0) (end {fnum(BOARD_W)} {fnum(BOARD_H)}) (stroke (width 0.1) (type default)) (fill none) (layer "Edge.Cuts") (uuid "{uid()}"))')
    texts = [
        ("PAC-TILT  ELF74", 64.38, 4.0, 1.5),
        ("USB", 64.38, 58.0, 1.2),
        ("ST7789 1.3in 240x240", 22.5, 22.5, 1.0),
        ("MPU6050", 91.2, 33.0, 1.0),
        ("B (pausa)", 88.0, 53.5, 1.0),
        ("A (confirma)", 104.0, 53.5, 1.0),
    ]
    for t, x, yy, s in texts:
        L.append(f'  (gr_text "{t}" (at {fnum(x)} {fnum(yy)} 0) (layer "F.SilkS") (uuid "{uid()}") (effects (font (size {s} {s}) (thickness 0.18))))')
    L.append(")")
    (OUT / f"{NAME}.kicad_pcb").write_text("\n".join(L) + "\n", encoding="utf-8")


# ---------------------------------------------------------------------------
# Escrita do .kicad_sch
# ---------------------------------------------------------------------------
FONT = "(effects (font (size 1.27 1.27)))"


def lib_symbol(name, left, right, half_w, ref_prefix):
    """left/right: listas de (numero, nome) de cima para baixo."""
    n = max(len(left), len(right))
    top = (n - 1) * P / 2
    h = top + P
    s = [f'    (symbol "pactilt:{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)',
         f'      (property "Reference" "{ref_prefix}" (at 0 {fnum(h + 1.27)} 0) {FONT})',
         f'      (property "Value" "{name}" (at 0 {fnum(-h - 1.27)} 0) {FONT})',
         f'      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
         f'      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
         f'      (property "Description" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))',
         f'      (symbol "{name}_0_1" (rectangle (start {fnum(-half_w)} {fnum(h)}) (end {fnum(half_w)} {fnum(-h)}) (stroke (width 0.254) (type default)) (fill (type background))))',
         f'      (symbol "{name}_1_1"']
    for i, (num, pname) in enumerate(left):
        s.append(f'        (pin passive line (at {fnum(-half_w - 5.08)} {fnum(top - i * P)} 0) (length 5.08) (name "{pname}" {FONT}) (number "{num}" {FONT}))')
    for i, (num, pname) in enumerate(right):
        s.append(f'        (pin passive line (at {fnum(half_w + 5.08)} {fnum(top - i * P)} 180) (length 5.08) (name "{pname}" {FONT}) (number "{num}" {FONT}))')
    s.append("      )")
    s.append("    )")
    pins = {}
    for i, (num, _) in enumerate(left):
        pins[num] = (-half_w - 5.08, top - i * P, "L")
    for i, (num, _) in enumerate(right):
        pins[num] = (half_w + 5.08, top - i * P, "R")
    return "\n".join(s), pins


def write_sch():
    syms = {
        "BlackPill_F411": lib_symbol("BlackPill_F411",
                                     [(str(i + 1), n) for i, n in enumerate(BP_LEFT)],
                                     [(str(i + 21), n) for i, n in enumerate(BP_RIGHT)], 10.16, "U"),
        "ST7789_Module": lib_symbol("ST7789_Module", [], [(str(i + 1), n) for i, (n, _) in enumerate(DISP_PINS)], 7.62, "J"),
        "MPU6050_Module": lib_symbol("MPU6050_Module", [(str(i + 1), n) for i, (n, _) in enumerate(MPU_PINS)], [], 7.62, "J"),
        "SW_Push": lib_symbol("SW_Push", [("1", "1")], [("4", "2")], 3.81, "SW"),
    }
    place = {
        "U1": ("BlackPill_F411", 139.7, 101.6),
        "J1": ("ST7789_Module", 55.88, 99.06),
        "J2": ("MPU6050_Module", 218.44, 76.2),
        "SW1": ("SW_Push", 218.44, 116.84),
        "SW2": ("SW_Push", 218.44, 132.08),
    }
    L = [f'(kicad_sch (version 20231120) (generator "eeschema") (generator_version "8.0") (uuid "{ROOT_UUID}") (paper "A4")',
         '  (title_block (title "Pac-Tilt - ELF74") (company "UTFPR") (comment 1 "BlackPill STM32F411 + ST7789 + MPU6050 + 2 botoes"))',
         '  (lib_symbols']
    L += [s for s, _ in syms.values()]
    L.append("  )")
    for ref, (sym, X, Y) in place.items():
        c = comps[ref]
        _, pins = syms[sym]
        L.append(f'  (symbol (lib_id "pactilt:{sym}") (at {fnum(X)} {fnum(Y)} 0) (unit 1) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid "{c["sym_uuid"]}")')
        top = max(p[1] for p in pins.values()) + P + 1.27
        L.append(f'    (property "Reference" "{ref}" (at {fnum(X)} {fnum(Y - top - 1.27)} 0) {FONT})')
        L.append(f'    (property "Value" "{c["value"]}" (at {fnum(X)} {fnum(Y + top + 1.27)} 0) {FONT})')
        L.append(f'    (property "Footprint" "pactilt:{c["fp"]}" (at {fnum(X)} {fnum(Y)} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        L.append(f'    (property "Datasheet" "" (at {fnum(X)} {fnum(Y)} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        L.append(f'    (property "Description" "" (at {fnum(X)} {fnum(Y)} 0) (effects (font (size 1.27 1.27)) (hide yes)))')
        for num in pins:
            L.append(f'    (pin "{num}" (uuid "{uid()}"))')
        L.append(f'    (instances (project "{NAME}" (path "/{ROOT_UUID}" (reference "{ref}") (unit 1))))')
        L.append("  )")
        for num, (px, py, side) in pins.items():
            ax, ay = X + px, Y - py
            net = c["pads"][num][1]
            if net:
                ang, just = (180, "right") if side == "L" else (0, "left")
                L.append(f'  (label "{net}" (at {fnum(ax)} {fnum(ay)} {ang}) (fields_autoplaced yes) (effects (font (size 1.27 1.27)) (justify {just} bottom)) (uuid "{uid()}"))')
            elif ref.startswith("SW") is False:
                L.append(f'  (no_connect (at {fnum(ax)} {fnum(ay)}) (uuid "{uid()}"))')
    notes = [
        ("Pinos A/B dos botoes: use cantos em diagonal (pads 1 e 4) da chave 12x12 mm.", 20.32, 170.18),
        ("BlackPill montada com o USB para BAIXO na placa. Firmware: env pcb (DC=PB1, RES=PB0).", 20.32, 175.26),
        ("3V3/G dos dois lados da BlackPill sao ligados internamente no modulo.", 20.32, 180.34),
    ]
    for t, x, yy in notes:
        L.append(f'  (text "{t}" (exclude_from_sim no) (at {fnum(x)} {fnum(yy)} 0) (effects (font (size 1.27 1.27)) (justify left bottom)) (uuid "{uid()}"))')
    L.append('  (sheet_instances (path "/" (page "1")))')
    L.append(")")
    (OUT / f"{NAME}.kicad_sch").write_text("\n".join(L) + "\n", encoding="utf-8")


def write_pro():
    pro = {
        "board": {
            "design_settings": {
                "defaults": {"board_outline_line_width": 0.1, "copper_line_width": 0.2},
                "rules": {"min_clearance": CLEARANCE, "min_track_width": TRACK,
                          "min_through_hole_diameter": 0.8, "min_copper_edge_clearance": 0.5},
                "track_widths": [0.0, TRACK],
                "via_dimensions": [{"diameter": 0.0, "drill": 0.0}],
            },
            "layer_presets": [], "viewports": [],
        },
        "meta": {"filename": f"{NAME}.kicad_pro", "version": 1},
        "net_settings": {
            "classes": [{
                "name": "Default", "clearance": CLEARANCE, "track_width": TRACK,
                "via_diameter": 0.8, "via_drill": 0.4, "microvia_diameter": 0.3, "microvia_drill": 0.1,
                "diff_pair_gap": 0.25, "diff_pair_width": 0.2, "diff_pair_via_gap": 0.25,
                "wire_width": 6, "bus_width": 12, "line_style": 0,
                "pcb_color": "rgba(0, 0, 0, 0.000)", "schematic_color": "rgba(0, 0, 0, 0.000)",
                "priority": 2147483647,
            }],
            "meta": {"version": 3},
        },
        "sheets": [[ROOT_UUID, "Root"]],
    }
    (OUT / f"{NAME}.kicad_pro").write_text(json.dumps(pro, indent=2) + "\n", encoding="utf-8")


def write_svg():
    """Previa da placa (vista de cima, cobre de baixo em transparencia)."""
    k = 8
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-20 -20 {BOARD_W * k + 40} {BOARD_H * k + 40}">',
         f'<rect x="-20" y="-20" width="{BOARD_W * k + 40}" height="{BOARD_H * k + 40}" fill="#fff"/>',
         f'<rect x="0" y="0" width="{BOARD_W * k}" height="{BOARD_H * k}" fill="#1d5c2e" stroke="#000" stroke-width="2"/>']
    for ref, (x0, y0, x1, y1) in OUTLINES.items():
        s.append(f'<rect x="{x0 * k}" y="{y0 * k}" width="{(x1 - x0) * k}" height="{(y1 - y0) * k}" fill="none" stroke="#eee" stroke-width="2" stroke-dasharray="6 4"/>')
        s.append(f'<text x="{x0 * k + 6}" y="{y0 * k + 18}" fill="#eee" font-size="16" font-family="Arial">{ref}</text>')
    for net, p in TRACKS:
        d = " ".join(f"{x * k},{yy * k}" for x, yy in p)
        s.append(f'<polyline points="{d}" fill="none" stroke="#e8a33d" stroke-width="{TRACK * k}" stroke-linejoin="round" stroke-linecap="round"/>')
    for ref, c in comps.items():
        for num, (name, net, x, yy, size, drill) in c["pads"].items():
            s.append(f'<circle cx="{x * k}" cy="{yy * k}" r="{size / 2 * k}" fill="{"#e8a33d" if net else "#b0b0b0"}"/>')
            s.append(f'<circle cx="{x * k}" cy="{yy * k}" r="{drill / 2 * k}" fill="#000"/>')
            if ref in ("U1", "J1", "J2") and net:
                s.append(f'<text x="{x * k + 9}" y="{yy * k + 4}" fill="#fff" font-size="10" font-family="Arial">{name}</text>')
    for hx, hy in HOLES:
        s.append(f'<circle cx="{hx * k}" cy="{hy * k}" r="{1.6 * k}" fill="#fff" stroke="#000"/>')
    s.append("</svg>")
    (OUT / "preview.svg").write_text("\n".join(s), encoding="utf-8")


if __name__ == "__main__":
    errs, worst = check()
    for e in errs:
        print("ERRO:", e)
    print(f"menor folga entre cobres de redes diferentes: {worst:.2f} mm (minimo {CLEARANCE})")
    write_pro()
    write_sch()
    write_pcb()
    write_svg()
    print("arquivos gerados em", OUT)
    raise SystemExit(1 if errs else 0)
