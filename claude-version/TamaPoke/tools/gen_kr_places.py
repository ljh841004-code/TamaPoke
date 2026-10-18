#!/usr/bin/env python3
"""ko12.10.13: tabla de los 시·군·구 de Corea (nombre + centro) para el tiempo real sin depender del
buscador de nombres (Open-Meteo no encuentra bien "영통구" o "수원").

Fuente: southkorea/southkorea-maps (KOSTAT 2018: skorea-municipalities-2018-geo.json y
skorea-provinces-2018-geo.json) + los cambios de despues, a mano (PATCH abajo).
Uso: python3 tools/gen_kr_places.py muni.json prov.json > kr_places.h
"""
import json
import sys


def ring_mean(ring):
    xs = [p[0] for p in ring]
    ys = [p[1] for p in ring]
    return sum(ys) / len(ys), sum(xs) / len(xs), len(ring)


def center(geom):
    polys = geom['coordinates'] if geom['type'] == 'MultiPolygon' else [geom['coordinates']]
    best = max((ring_mean(p[0]) for p in polys), key=lambda t: t[2])  # el poligono mas grande
    return best[0], best[1]


# cambios despues de 2018: nombres nuevos de provincia (el viejo se queda para buscar), 미추홀구, 군위군 a
# Daegu (2023), los 3 구 de Bucheon (2024) y los 4 de Hwaseong (2026); centros aproximados
PROV_NEW = {'강원도': '강원특별자치도·강원도', '전라북도': '전북특별자치도·전라북도'}
RENAME = {('인천광역시', '남구'): '미추홀구'}
MOVE = {('경상북도', '군위군'): '대구광역시'}
DROP = {('경기도', '부천시'), ('경기도', '화성시')}
ADD = [('경기도', '부천시원미구', 37.50, 126.78), ('경기도', '부천시소사구', 37.48, 126.80),
       ('경기도', '부천시오정구', 37.52, 126.80),
       ('경기도', '화성시동탄구', 37.20, 127.07), ('경기도', '화성시병점구', 37.21, 127.03),
       ('경기도', '화성시효행구', 37.19, 126.95), ('경기도', '화성시만세구', 37.15, 126.83)]
muni = json.load(open(sys.argv[1]))['features']
prov = {f['properties']['code']: f['properties']['name'] for f in json.load(open(sys.argv[2]))['features']}
rows = []
for f in muni:
    code = f['properties']['code']
    la, lo = center(f['geometry'])
    pv, nm = prov.get(code[:2], ''), f['properties']['name']
    if (pv, nm) in DROP:
        continue
    nm = RENAME.get((pv, nm), nm)
    pv = MOVE.get((pv, nm), pv)
    rows.append((code, PROV_NEW.get(pv, pv), nm, round(la * 100), round(lo * 100)))
for i, (pv, nm, la, lo) in enumerate(ADD):
    rows.append(('99%03d' % i, PROV_NEW.get(pv, pv), nm, round(la * 100), round(lo * 100)))
rows.sort()
provs = sorted(set(r[1] for r in rows))
print('#pragma once')
print('// ko12.10.13: generado por tools/gen_kr_places.py (KOSTAT 2018 via southkorea-maps + cambios hasta 2026). No editar a mano')
print('#include <stdint.h>')
print('static const char *const KR_PROV[] = { %s };' % ', '.join('"%s"' % p for p in provs))
print('struct KrPlace { uint8_t prov; const char *name; int16_t lat100, lon100; };')
print('static const KrPlace KR_PLACES[] = {')
for code, p, n, la, lo in rows:
    print('  { %d, "%s", %d, %d },' % (provs.index(p), n, la, lo))
print('};')
print('#define KR_PLACES_N (sizeof(KR_PLACES) / sizeof(KR_PLACES[0]))')
