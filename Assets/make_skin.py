#!/usr/bin/env python3
"""Prepara a arte aprovada (metal escovado) para o plugin.

    python3 Assets/make_skin.py [Assets/src/arte_aprovada.png]

Gera em Assets/: skin_bg.jpg (arte sem ponteiros, arcos, valores e medidor),
knob.png (tampa do knob sem ponteiro), btn_dark/cream/red.png (teclas sem texto)
e Source/SkinLayout.h (posicoes medidas na arte). Precisa de numpy e opencv.
"""
import os, sys
import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'src', 'arte_aprovada.png')
DBG = os.environ.get('SKIN_DEBUG')

art = cv2.imread(SRC)
assert art is not None and art.shape[:2] == (971, 1619), 'arte esperada: 1619 x 971'
H_, W_ = art.shape[:2]
gray0 = cv2.cvtColor(art, cv2.COLOR_BGR2GRAY)
hsv0 = cv2.cvtColor(art, cv2.COLOR_BGR2HSV)

# knobs: (centro x, centro y, raio do aro da tampa), medidos por casamento com o knob WIDTH
KNOBS = {
    'link': (513, 304, 34.7), 'thresh': (769, 310, 42.5), 'ratio': (909, 312, 43.0), 'attack': (1049, 312, 43.0),
    'release': (1189, 312, 43.4), 'satAmount': (1447, 303, 36.8), 'scHpf': (448, 466, 34.3), 'scTilt': (582, 465, 34.3),
    'makeup': (769, 496, 42.5), 'knee': (909, 496, 42.5), 'range': (1049, 495, 43.0), 'compMix': (1189, 495, 42.5),
    'satTone': (1374, 464, 34.7), 'satMix': (1516, 465, 34.3), 'air': (473, 825, 40.9), 'width': (636, 824, 41.3),
    'monoBass': (800, 824, 41.7), 'output': (1489, 823, 41.7),
}
MASTER = 'width'
BUTTONS = {
    'extSc': (443, 580, 590, 618), 'autoRel': (946, 608, 1079, 646), 'sync': (1092, 608, 1250, 646),
    'satTom': (1313, 563, 1440, 601), 'satHarm': (1454, 563, 1582, 601), 'satPre': (1313, 612, 1582, 651),
    'spatOn': (946, 807, 1064, 851), 'compOn': (1085, 807, 1202, 851), 'satOn': (1223, 807, 1339, 851),
}
PRESET = (37, 842, 329, 886)
SEG_X0, SEG_PITCH, SEG_W, SEG_Y0, SEG_Y1, SEG_N = 571.0, 28.0, 24.5, 24.0, 51.0, 36
GR_X0, GR_X20, GR_Y = 572.5, 1552.5, 85.5

bg = art.copy()

def inpaint(img, mask, radius=4):
    return cv2.inpaint(img, mask, radius, cv2.INPAINT_TELEA)

#------------------------------------------------------------------ tampa mestre (sem ponteiro)
mx, my, mr = KNOBS[MASTER]
HS = 56
crop = art[my - HS:my + HS + 1, mx - HS:mx + HS + 1].copy()
yy, xx = np.mgrid[-HS:HS + 1, -HS:HS + 1]
rr = np.hypot(xx, yy)
ch = cv2.cvtColor(crop, cv2.COLOR_BGR2HSV)
red = (((ch[:, :, 0] < 12) | (ch[:, :, 0] > 168)) & (ch[:, :, 1] > 90) & (ch[:, :, 2] > 60) & (rr < mr - 1)).astype(np.uint8)
red = cv2.dilate(red, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9)))
rot = cv2.rotate(crop, cv2.ROTATE_180)
soft = cv2.GaussianBlur(red.astype(np.float32), (0, 0), 1.5)[:, :, None]
cap = (crop * (1 - soft) + rot * soft).astype(np.uint8)
# alfa: tampa + saia (a saia desce um pouco mais embaixo)
edge = mr + 4.0 + 3.5 * np.clip(yy / mr, 0, 1)
alpha = np.clip((edge + 2.5 - rr) / 3.5, 0, 1)
knob = np.dstack([cap, (alpha * 255).astype(np.uint8)])
knob = cv2.resize(knob, None, fx=2, fy=2, interpolation=cv2.INTER_LANCZOS4)
cv2.imwrite(os.path.join(HERE, 'knob.png'), knob)

#------------------------------------------------------------------ valores embaixo dos knobs
VALUE_Y = {}
tmask = np.zeros((H_, W_), np.uint8)
for n, (cx, cy, r) in KNOBS.items():
    y0 = int(cy + r * 1.5) - 6
    band = gray0[y0:y0 + 46, cx - 58:cx + 59]
    rows = np.flatnonzero((band > 150).sum(axis=1) > 0)
    # primeiro bloco de linhas claras = o valor
    first = rows[0]; last = first
    for q in rows[1:]:
        if q - last > 3: break
        last = q
    ya, yb = y0 + first, y0 + last
    VALUE_Y[n] = (ya + yb) / 2.0
    box = np.zeros_like(tmask); box[ya - 4:yb + 5, cx - 62:cx + 63] = 1
    tmask |= ((gray0 > 70) & (box > 0)).astype(np.uint8) * 255

def text_box(x0, y0, x1, y1, thr=70):
    box = np.zeros_like(tmask); box[y0:y1, x0:x1] = 1
    return ((gray0 > thr) & (box > 0)).astype(np.uint8) * 255

tmask |= text_box(384, 102, 450, 134)                    # leitura "-6.4"
tmask |= text_box(884, 94, 899, 103, 60)                 # ponta do marcador de pico
tmask |= text_box(PRESET[0] + 8, PRESET[1] + 5, PRESET[2] - 50, PRESET[3] - 5)   # nome do preset
tmask = cv2.dilate(tmask, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7)))
bg = inpaint(bg, tmask, 5)

#------------------------------------------------------------------ fundo: apaga knobs (o plugin redesenha)
mask = np.zeros((H_, W_), np.uint8)
g1 = cv2.cvtColor(bg, cv2.COLOR_BGR2GRAY); h1 = cv2.cvtColor(bg, cv2.COLOR_BGR2HSV)
lit = (g1 > 75) | ((h1[:, :, 1] > 110) & (h1[:, :, 2] > 70))
for n, (cx, cy, r) in KNOBS.items():
    cv2.circle(mask, (cx, int(round(cy + 0.10 * r))), int(round(r * 1.42)), 255, -1)
    wide = np.zeros_like(mask); cv2.circle(wide, (cx, int(round(cy + 0.10 * r))), int(round(r * 1.66)), 255, -1)
    wide[:int(cy - 1.30 * r)] = 0
    mask |= cv2.dilate(((wide > 0) & lit).astype(np.uint8) * 255, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7)))
bg = inpaint(bg, mask, 6)
# devolve um pouco do escovado horizontal na area apagada
rng = np.random.default_rng(7)
streak = cv2.GaussianBlur(rng.normal(0, 1, (H_, W_)).astype(np.float32), (0, 0), sigmaX=14, sigmaY=0.6)
streak = streak / streak.std() * 3.2
soft = cv2.GaussianBlur(mask.astype(np.float32) / 255, (0, 0), 3)[:, :, None]
bg = np.clip(bg.astype(np.float32) + soft * streak[:, :, None], 0, 255).astype(np.uint8)

#------------------------------------------------------------------ medidor: tira a barra vermelha e o marcador
src = bg[66:97, 566 + 420:900 + 420].copy()
dst = bg[66:97, 566:900]
ramp = np.ones((31, 334, 1), np.float32)
ramp[:, :8] = np.linspace(0, 1, 8)[None, :, None]; ramp[:, -8:] = np.linspace(1, 0, 8)[None, :, None]
ramp[:5] *= np.linspace(0, 1, 5)[:, None, None]; ramp[-5:] *= np.linspace(1, 0, 5)[:, None, None]
bg[66:97, 566:900] = (dst * (1 - ramp) + src * ramp).astype(np.uint8)
# segmentos do DRIVE: todos apagados no fundo (o plugin acende)
for k in range(SEG_N):
    x = SEG_X0 + k * SEG_PITCH
    hot = k >= SEG_N - 5
    col = (20, 16, 74) if hot else (60, 62, 66)
    cv2.rectangle(bg, (int(round(x)), int(SEG_Y0)), (int(round(x + SEG_W)), int(SEG_Y1)), (8, 8, 8), -1)
    cv2.rectangle(bg, (int(round(x)) + 1, int(SEG_Y0) + 1), (int(round(x + SEG_W)) - 1, int(SEG_Y1) - 1), col, -1)

cv2.imwrite(os.path.join(HERE, 'skin_bg.jpg'), bg, [cv2.IMWRITE_JPEG_QUALITY, 94])

#------------------------------------------------------------------ teclas sem texto
def plate(name, kind, out):
    x0, y0, x1, y1 = BUTTONS[name]
    m = 6
    c = art[y0 - m:y1 + m + 1, x0 - m:x1 + m + 1].copy()
    g = cv2.cvtColor(c, cv2.COLOR_BGR2GRAY); h = cv2.cvtColor(c, cv2.COLOR_BGR2HSV)
    # miolo sem texto: cada linha vira um degrade entre as duas bordas limpas da tecla
    xa, xb = m + 9, c.shape[1] - m - 9
    left = c[:, xa - 3:xa].astype(np.float32).mean(axis=1, keepdims=True)
    right = c[:, xb:xb + 3].astype(np.float32).mean(axis=1, keepdims=True)
    t = np.linspace(0, 1, xb - xa)[None, :, None]
    fill = left * (1 - t) + right * t
    rows = slice(m + 5, c.shape[0] - m - 5)
    c[rows, xa:xb] = fill[rows].astype(np.uint8)
    cv2.imwrite(os.path.join(HERE, out), c)
    return m

BM = plate('autoRel', 'dark', 'btn_dark.png')
plate('satTom', 'cream', 'btn_cream.png')
plate('compOn', 'red', 'btn_red.png')

#------------------------------------------------------------------ SkinLayout.h
L = ['// Gerado por Assets/make_skin.py - posicoes medidas na arte aprovada (pixels da arte, 1619 x 971).',
     '#pragma once', '', 'namespace skin', '{',
     f'constexpr float kArtW = {W_}.0f, kArtH = {H_}.0f;',
     f'constexpr float kKnobSpriteRim = {mr * 2:.1f}f;      // raio do aro dentro de knob.png',
     f'constexpr int   kButtonMargin = {BM};',
     f'constexpr float kSegX0 = {SEG_X0}f, kSegPitch = {SEG_PITCH}f, kSegW = {SEG_W}f, kSegY0 = {SEG_Y0}f, kSegY1 = {SEG_Y1}f;',
     f'constexpr int   kSegCount = {SEG_N}, kSegHot = 5;',
     f'constexpr float kGrX0 = {GR_X0}f, kGrX20 = {GR_X20}f, kGrY = {GR_Y}f;',
     'struct KnobPos { const char* id; float cx, cy, rim, valueY; };',
     'constexpr KnobPos kKnobs[] = {']
for n, (cx, cy, r) in KNOBS.items():
    L.append(f'    {{ "{n}", {cx}.0f, {cy}.0f, {r}f, {VALUE_Y[n]:.1f}f }},')
L += ['};', 'struct RectPos { const char* id; float x0, y0, x1, y1; };', 'constexpr RectPos kButtons[] = {']
for n, b in BUTTONS.items():
    L.append(f'    {{ "{n}", {b[0]}.0f, {b[1]}.0f, {b[2]}.0f, {b[3]}.0f }},')
L += ['};', f'constexpr RectPos kPreset {{ "preset", {PRESET[0]}.0f, {PRESET[1]}.0f, {PRESET[2]}.0f, {PRESET[3]}.0f }};', '} // namespace skin', '']
open(os.path.join(HERE, '..', 'Source', 'SkinLayout.h'), 'w').write('\n'.join(L))
print('ok', {k: round(v, 1) for k, v in VALUE_Y.items()})
