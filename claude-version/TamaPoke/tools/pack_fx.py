#!/usr/bin/env python3
"""ko11.30: efectos de los ataques con las animaciones de pokerogue.net.

Para cada ataque (16 tipos x 3 fases x 3 variantes, nombres de i18n_ext.cpp)
descarga la animacion (battle-anims/<ataque>.json) y sus graficos
(images/battle_anims/*.png), y compone cada fotograma YA en coordenadas de la
pantalla redonda de 466x466 para los dos sentidos:
  lado 0 = ataca el mio  (140,200) -> rival (316,116)
  lado 1 = ataca el rival (316,116) -> mio  (140,200)
           (si el json trae version del rival, esa ya viene en el espacio del lado 0)
El firmware solo pega los fotogramas: nada de girar ni escalar en el ESP32.

Formato (mons/fx/fTTSV.bin; TT = tipo, S = fase, V = variante):
  "TFX2" u8 lados u16 ms_por_fotograma u8 nfondos
  fondo: u16 w, u16 h, u16 npal, npal * (u16 c565, u8 a 0..32), w*h indices
         (color normal, sin premultiplicar; se repite en mosaico)
  por lado: u16 n, u32 offset[n]
  fotograma: u8 fondo (0xFF = ninguno), i16 bx, i16 by, u16 bw, u16 bh,
             u16 escala_x*256, u16 escala_y*256, u8 alfa 0..32,
             i16 x, i16 y, u16 w, u16 h, u8 npal, npal * (u16 c565, u8 a 0..32),
             u32 len, datos RLE: b<128 -> b+1 transparentes; b>=128 -> b-127 indices
  El color es premultiplicado: pantalla = c + fondo * (32 - a) / 32 (asi sirve
  tambien para la mezcla aditiva de los destellos).

Los graficos y animaciones son de PokeRogue / Nintendo / Game Freak: SOLO USO
PERSONAL. No se suben al repositorio (tools/fx_cache y tools/sdcard estan en
.gitignore).

Uso: python3 pack_fx.py              (todos)
     python3 pack_fx.py 1 2 3 4      (solo esos tipos)
     FX_OUT=/ruta python3 pack_fx.py
"""
import json
import math
import os
import re
import struct
import sys
import urllib.parse
import urllib.request

from PIL import Image

BASE = 'https://pokerogue.net'
HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'fx_cache')
OUT = os.environ.get('FX_OUT') or os.path.join(HERE, 'sdcard', 'mons', 'fx')
SRC = os.path.join(HERE, '..', 'i18n_ext.cpp')

W = H = 466
SIDES = [((140, 200), (316, 116)), ((316, 116), (140, 200))]
A_T = (128.0, -64.0)        # objetivo en el espacio de la animacion (usuario en 0,0)
FRAME_MS = 50               # PokeRogue: 3 fotogramas de 60 Hz
MAX_MS = 1300               # el golpe dura 1400 ms: las largas se aceleran
CELL = 96
CLIP_Y = 262                # no pinta encima del cuadro de texto

# nombres antiguos (MAYUSCULAS juntas) -> nombre de PokeRogue
ALIAS = {
    'THUNDERSHOCK': 'thunder-shock', 'SOLARBEAM': 'solar-beam', 'DYNAMICPUNCH': 'dynamic-punch',
    'ANCIENTPOWER': 'ancient-power', 'FAINT ATTACK': 'feint-attack', 'DRAGONBREATH': 'dragon-breath',
    'THUNDERPUNCH': 'thunder-punch', 'SMOKESCREEN': 'smokescreen', 'SELFDESTRUCT': 'self-destruct',
    'HI JUMP KICK': 'high-jump-kick', 'DOUBLESLAP': 'double-slap', 'SONICBOOM': 'sonic-boom',
    'POISONPOWDER': 'poison-powder', 'SOFTBOILED': 'soft-boiled', 'VICEGRIP': 'vise-grip',
    'EXTREMESPEED': 'extreme-speed', 'DRAGON RAGE': 'dragon-rage',
}


def slug(name):
    if name in ALIAS:
        return ALIAS[name]
    return re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')


def move_names():
    """[(tipo, fase, variante, nombre EN)] leidos de i18n_ext.cpp."""
    src = open(SRC, encoding='utf-8').read()
    def block(tag):
        i = src.index(tag)
        j = src.index('};', i)
        return re.findall(r'"([^"]*)"', src[i:j])
    m1 = block('MOVES_EN[3][PT_COUNT] =')
    m2 = block('MOVES2_EN[3][PT_COUNT][2] =')
    out = []
    for s in range(3):
        for t in range(16):
            out.append((t, s, 0, m1[s * 16 + t]))
            out.append((t, s, 1, m2[(s * 16 + t) * 2]))
            out.append((t, s, 2, m2[(s * 16 + t) * 2 + 1]))
    return out


def fetch(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return True
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'TamaPoke-pack_fx'})
        data = urllib.request.urlopen(req, timeout=30).read()
    except Exception:
        return False
    if data[:1] == b'<':  # pagina 404
        return False
    open(dest, 'wb').write(data)
    return True


_sheets = {}


def sheet(graphic):
    if graphic not in _sheets:
        dest = os.path.join(CACHE, 'img', graphic + '.png')
        url = BASE + '/images/battle_anims/' + urllib.parse.quote(graphic) + '.png'
        _sheets[graphic] = Image.open(dest).convert('RGBA') if fetch(url, dest) else None
    return _sheets[graphic]


def cell(sh, idx):
    cols = sh.width // CELL
    if cols <= 0:
        return None
    x, y = (idx % cols) * CELL, (idx // cols) * CELL
    if y + CELL > sh.height:
        return None
    return sh.crop((x, y, x + CELL, y + CELL))


def place(e, A, T):
    """posicion en pantalla del centro del grafico segun su foco."""
    x, y = float(e.get('x', 0)), float(e.get('y', 0))
    sx, sy = (T[0] - A[0]) / A_T[0], (T[1] - A[1]) / A_T[1]
    f = e.get('focus', 3)
    if f == 1:    # usuario
        return A[0] + x * abs(sx), A[1] + y * abs(sy)
    if f == 2:    # objetivo
        return T[0] + (x - A_T[0]) * abs(sx), T[1] + (y - A_T[1]) * abs(sy)
    return A[0] + x * sx, A[1] + y * sy   # 3 = linea usuario->objetivo (y 4)


def compose(anim, fi, A, T, scale):
    """devuelve (premultiplicado RGB float, alfa float) como listas de Image 'F'."""
    import numpy as np
    col = np.zeros((H, W, 3), np.float32)
    alp = np.zeros((H, W), np.float32)
    sh = sheet(anim['graphic']) if anim.get('graphic') else None
    if sh is None:
        return col, alp
    for e in anim['frames'][fi]:
        if e.get('target') != 2 or not e.get('visible', True):
            continue
        c = cell(sh, int(e.get('graphicFrame', 0)))
        if c is None:
            continue
        zx = e.get('zoomX', 100) / 100.0 * scale
        zy = e.get('zoomY', 100) / 100.0 * scale
        if zx <= 0.01 or zy <= 0.01:
            continue
        if e.get('mirror'):
            c = c.transpose(Image.FLIP_LEFT_RIGHT)
        c = c.resize((max(1, int(CELL * zx)), max(1, int(CELL * zy))), Image.BILINEAR)
        ang = e.get('angle', 0)
        if ang:
            c = c.rotate(ang, Image.BILINEAR, expand=True)
        cx, cy = place(e, A, T)
        x0, y0 = int(round(cx - c.width / 2)), int(round(cy - c.height / 2))
        a = np.asarray(c, np.float32) / 255.0
        op = e.get('opacity', 255) / 255.0
        # recorte contra la pantalla
        sx0, sy0 = max(0, -x0), max(0, -y0)
        dx0, dy0 = max(0, x0), max(0, y0)
        w = min(c.width - sx0, W - dx0)
        h = min(c.height - sy0, H - dy0)
        if w <= 0 or h <= 0:
            continue
        src = a[sy0:sy0 + h, sx0:sx0 + w]
        sa = src[..., 3] * op
        sc = src[..., :3] * sa[..., None]
        dc = col[dy0:dy0 + h, dx0:dx0 + w]
        da = alp[dy0:dy0 + h, dx0:dx0 + w]
        blend = e.get('blendType', 0)
        if blend == 1:      # aditiva: suma luz, no tapa el fondo
            dc += sc
        else:               # normal (y la resta, que es rara, como normal)
            dc *= (1 - sa)[..., None]
            dc += sc
            da *= (1 - sa)
            da += sa
    np.clip(col, 0, 1, out=col)
    return col, alp


def encode(col, alp):
    import numpy as np
    col[CLIP_Y:] = 0
    alp[CLIP_Y:] = 0
    vis = (alp > 0.03) | (col.max(axis=2) > 0.03)
    if not vis.any():
        return struct.pack('<hhHHB', 0, 0, 0, 0, 0) + struct.pack('<I', 0)
    ys, xs = np.where(vis)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    c = col[y0:y1, x0:x1]
    a = alp[y0:y1, x0:x1]
    v = vis[y0:y1, x0:x1]
    h, w = a.shape
    # cuantiza color (premultiplicado) y alfa por separado: PIL junta en una
    # sola entrada todo lo de alfa 0, y la luz aditiva tiene alfa 0
    rgb = Image.fromarray((c * 255 + 0.5).astype(np.uint8), 'RGB')
    a5 = (a * 32 + 0.5).astype(np.int32)
    for ncol, astep in ((64, 1), (48, 2), (32, 2), (24, 4), (16, 4), (8, 8)):
        q = rgb.quantize(colors=ncol, method=Image.Quantize.MEDIANCUT)
        ci = np.asarray(q, np.int32)
        ak = (a5 + astep // 2) // astep * astep
        ak[ak > 32] = 32
        key = ci * 33 + ak
        uk, inv = np.unique(key[v], return_inverse=True)
        if len(uk) <= 255:
            break
    qp = q.getpalette()[:3 * ncol]
    npal = len(uk)
    pal_b = b''
    for k in uk:
        ci_, al = divmod(int(k), 33)
        r, g, b = qp[3 * ci_:3 * ci_ + 3]
        pal_b += struct.pack('<HB', ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3), al)
    idx = np.zeros(a.shape, np.uint8)
    idx[v] = inv.astype(np.uint8)
    flat_i = idx.reshape(-1)
    flat_v = v.reshape(-1)
    data = bytearray()
    n = len(flat_i)
    i = 0
    while i < n:
        if not flat_v[i]:
            j = i
            while j < n and not flat_v[j] and j - i < 128:
                j += 1
            data.append(j - i - 1)
        else:
            j = i
            while j < n and flat_v[j] and j - i < 128:
                j += 1
            data.append(127 + j - i)
            data += bytes(flat_i[i:j])
        i = j
    return (struct.pack('<hhHHB', int(x0), int(y0), int(w), int(h), npal) + pal_b +
            struct.pack('<I', len(data)) + bytes(data))


# pantalla de PokeRogue (320x180) -> la nuestra, con los puntos de enfoque
PR_USER = (106.0, 116.0)
KX = (SIDES[0][1][0] - SIDES[0][0][0]) / A_T[0]
KY = (SIDES[0][1][1] - SIDES[0][0][1]) / A_T[1]
BG_TILE = (896, 576)
BG_SCALE = 1.25
BG_MAX_A = 0.8            # el fondo negro del todo taparia a los Pokemon


def pr2scr(x, y):
    return SIDES[0][0][0] + (x - PR_USER[0]) * KX, SIDES[0][0][1] + (y - PR_USER[1]) * KY


def bg_image(name):
    if not name:
        im = Image.new('RGBA', (8, 8), (0, 0, 0, 255))
    else:
        dest = os.path.join(CACHE, 'img', name + '.png')
        url = BASE + '/images/battle_anims/' + urllib.parse.quote(name) + '.png'
        if not fetch(url, dest):
            return None
        im = Image.open(dest).convert('RGBA')
    q = im.quantize(colors=255, method=Image.Quantize.FASTOCTREE)
    pal = q.getpalette(rawmode='RGBA')
    npal = len(pal) // 4
    b = struct.pack('<HHH', im.width, im.height, npal)
    for i in range(npal):
        r, g, bl, al = pal[4 * i:4 * i + 4]
        b += struct.pack('<HB', ((r >> 3) << 11) | ((g >> 2) << 5) | (bl >> 3), (al * 32 + 127) // 255)
    return b + q.tobytes()


def bg_track(anim, nfr):
    """estado del fondo en cada fotograma: (nombre, x, y, alfa 0..1) o None."""
    ev = {}
    for k, lst in anim.get('frameTimedEvents', {}).items():
        for e in lst:
            if e['eventType'] in ('AnimTimedAddBgEvent', 'AnimTimedUpdateBgEvent'):
                ev.setdefault(int(k), []).append(e)
    cur = None
    tweens = []   # (prop, desde, hasta, f0, f1)
    out = []
    for f in range(nfr):
        for e in ev.get(f, []):
            if e['eventType'] == 'AnimTimedAddBgEvent':
                cur = {'name': e.get('resourceName', ''), 'x': e.get('bgX', 0) - 320.0,
                       'y': e.get('bgY', 0) - 284.0, 'a': e.get('opacity', 0) / 255.0}
                tweens = []
            elif cur is not None:
                d = max(1, int(e.get('duration', 1)))
                tg = {}
                if 'bgX' in e: tg['x'] = e['bgX'] - 320.0
                if 'bgY' in e: tg['y'] = e['bgY'] - 284.0
                if 'opacity' in e: tg['a'] = e['opacity'] / 255.0
                tweens = [tw for tw in tweens if tw[0] not in tg]
                for kk, vv in tg.items():
                    tweens.append((kk, cur[kk], vv, f, f + d))
        if cur is not None:
            for kk, a0, a1, f0, f1 in tweens:
                q = min(1.0, max(0.0, (f - f0) / float(f1 - f0)))
                cur[kk] = a0 + (a1 - a0) * q
            out.append(dict(cur))
        else:
            out.append(None)
    return out


def pack(t, s, v, name, fname=None):
    sl = slug(name)
    dest = os.path.join(CACHE, 'anims', sl + '.json')
    if not fetch(BASE + '/battle-anims/' + sl + '.json', dest):
        return None
    anims = json.load(open(dest))
    if isinstance(anims, dict):
        anims = [anims]
    nfr = max(len(a['frames']) for a in anims)
    ms = FRAME_MS if nfr * FRAME_MS <= MAX_MS else max(20, MAX_MS // nfr)
    bgs, bgdata = [], []
    sides = []
    useful = False
    for side, (A, T) in enumerate(SIDES):
        anim = anims[min(side, len(anims) - 1)]
        # la version "del rival" de PokeRogue esta dibujada desde el lado del
        # jugador: su origen (0,0) es el Pokemon de abajo, no el que ataca
        if side == 1 and len(anims) > 1:
            A, T = SIDES[0]
        dist = math.hypot(T[0] - A[0], T[1] - A[1])
        scale = dist / math.hypot(*A_T)
        track = bg_track(anim, len(anim['frames']))
        frames = []
        for fi in range(len(anim['frames'])):
            st = track[fi]
            bgi, bh = 0xFF, struct.pack('<BhhHHHHB', 0xFF, 0, 0, 0, 0, 0, 0, 0)
            if st is not None and st['a'] > 0.02:
                if st['name'] not in bgs:
                    img = bg_image(st['name'])
                    if img is not None:
                        bgs.append(st['name'])
                        bgdata.append(img)
                if st['name'] in bgs:
                    bgi = bgs.index(st['name'])
                    x0, y0 = pr2scr(st['x'], st['y'])
                    sx, sy = BG_SCALE * KX, BG_SCALE * abs(KY)
                    bw, bhh = BG_TILE[0] * sx, BG_TILE[1] * sy
                    bh = struct.pack('<BhhHHHHB', bgi, int(x0), int(y0), int(bw), int(bhh),
                                     int(sx * 256), int(sy * 256), int(min(st['a'], BG_MAX_A) * 32 + 0.5))
                    useful = True
            fg = encode(*compose(anim, fi, A, T, scale))
            if fg[4:8] != b'\0\0\0\0':
                useful = True
            frames.append(bh + fg)
        sides.append(frames)
    if not useful:   # sin graficos (p. ej. solo tinta al objetivo): mejor el efecto propio
        return 'empty'
    hdr = b'TFX2' + struct.pack('<BHB', len(sides), ms, len(bgdata)) + b''.join(bgdata)
    off = len(hdr) + sum(2 + 4 * len(f) for f in sides)
    tables = b''
    for frames in sides:
        tables += struct.pack('<H', len(frames))
        for fr in frames:
            tables += struct.pack('<I', off)
            off += len(fr)
    body = b''.join(b''.join(f) for f in sides)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, fname or 'f%02d%d%d.bin' % (t, s, v))
    open(path, 'wb').write(hdr + tables + body)
    return path, nfr, ms, os.path.getsize(path)


# ko11.31: placaje y los de estado (ids 145.. de tools/gen_moves.py): mNNN.bin
EXTRA = ['tackle'] + ['swords-dance', 'iron-defense', 'agility', 'growth', 'harden', 'growl', 'tail-whip',
                      'leer', 'string-shot', 'scary-face', 'screech', 'metal-sound', 'poison-powder', 'toxic',
                      'poison-gas', 'thunder-wave', 'stun-spore', 'glare', 'sleep-powder', 'hypnosis', 'sing',
                      'spore', 'will-o-wisp', 'confuse-ray', 'supersonic', 'sweet-kiss']


def main():
    types = {int(a) for a in sys.argv[1:] if a.isdigit()} if len(sys.argv) > 1 else None
    miss = []
    total = 0
    if types is None or 'extra' in sys.argv:
        for k, sl in enumerate(EXTRA):
            mid = 145 + k
            r = pack(0, 0, 0, sl, 'm%03d.bin' % mid)
            if r is None or r == 'empty':
                miss.append(sl)
                print('  -- sin animacion:', sl)
                continue
            total += r[3]
            print('  %-14s m%03d.bin  %2d fot. x %d ms  %4d KB' % (sl, mid, r[1], r[2], r[3] // 1024))
        if types is None:
            types = set(range(16))
    for t, s, v, name in move_names():
        if types is not None and t not in types:
            continue
        r = pack(t, s, v, name)
        if r is None or r == 'empty':
            miss.append('%s (%s)' % (name, slug(name)))
            print('  -- sin animacion:', name)
            path = os.path.join(OUT, 'f%02d%d%d.bin' % (t, s, v))
            if os.path.exists(path):
                os.remove(path)
            continue
        total += r[3]
        print('  %-14s f%02d%d%d.bin  %2d fot. x %d ms  %4d KB' % (name, t, s, v, r[1], r[2], r[3] // 1024))
    print('total %d KB en %s' % (total // 1024, OUT))
    if miss:
        print('sin animacion (el firmware usa su propio efecto):', ', '.join(miss))


if __name__ == '__main__':
    main()
