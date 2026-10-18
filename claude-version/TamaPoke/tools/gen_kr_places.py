#!/usr/bin/env python3
"""ko12.10.13: tabla de los 시·군·구 de Corea (nombre + centro) para el tiempo real sin depender del
buscador de nombres (Open-Meteo no encuentra bien "영통구" o "수원").

Fuente: southkorea/southkorea-maps (KOSTAT 2013, municipios y provincias, geojson simplificado).
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


muni = json.load(open(sys.argv[1]))['features']
prov = {f['properties']['code']: f['properties']['name'] for f in json.load(open(sys.argv[2]))['features']}
rows = []
for f in muni:
    code = f['properties']['code']
    la, lo = center(f['geometry'])
    rows.append((code, prov.get(code[:2], ''), f['properties']['name'], round(la * 100), round(lo * 100)))
rows.sort()
provs = sorted(set(r[1] for r in rows))
print('#pragma once')
print('// ko12.10.13: generado por tools/gen_kr_places.py (KOSTAT 2013 via southkorea-maps). No editar a mano')
print('#include <stdint.h>')
print('static const char *const KR_PROV[] = { %s };' % ', '.join('"%s"' % p for p in provs))
print('struct KrPlace { uint8_t prov; const char *name; int16_t lat100, lon100; };')
print('static const KrPlace KR_PLACES[] = {')
for code, p, n, la, lo in rows:
    print('  { %d, "%s", %d, %d },' % (provs.index(p), n, la, lo))
print('};')
print('#define KR_PLACES_N (sizeof(KR_PLACES) / sizeof(KR_PLACES[0]))')
