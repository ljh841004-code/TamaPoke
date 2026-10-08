#!/usr/bin/env python3
"""ko12.8: ataque especial y defensa especial de cada especie (dex_special.h), de PokeAPI.

Antes las batallas solo tenian ataque y defensa: los atacantes especiales (Alakazam,
Gengar, Espeon...) pegaban con su ataque fisico, que es bajo. Con esta tabla el
firmware usa la proporcion especial/fisico de cada especie (ver battle.cpp spStat).

Uso: python3 tools/gen_special.py [pokemon_stats.csv]   -> ../dex_special.h
Sin fichero, baja el CSV de PokeAPI (data/v2/csv/pokemon_stats.csv). Solo numeros.
"""
import csv
import io
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'dex_special.h')
URL = 'https://raw.githubusercontent.com/PokeAPI/pokeapi/master/data/v2/csv/pokemon_stats.csv'
N = 251


def load(path):
    if path:
        with open(path, encoding='utf-8') as f:
            return f.read()
    with urllib.request.urlopen(URL, timeout=60) as r:
        return r.read().decode('utf-8')


def main():
    text = load(sys.argv[1] if len(sys.argv) > 1 else None)
    spa, spd = [0] * (N + 1), [0] * (N + 1)
    for row in csv.DictReader(io.StringIO(text)):
        pid, sid = int(row['pokemon_id']), int(row['stat_id'])
        if 1 <= pid <= N:
            if sid == 4:
                spa[pid] = int(row['base_stat'])
            elif sid == 5:
                spd[pid] = int(row['base_stat'])
    missing = [i for i in range(1, N + 1) if not spa[i] or not spd[i]]
    if missing:
        raise SystemExit(f'faltan datos: {missing[:10]}')

    def arr(v):
        rows = [', '.join(str(x) for x in v[i:i + 16]) for i in range(0, len(v), 16)]
        return '  ' + ',\n  '.join(rows)

    with open(OUT, 'w') as f:
        f.write('#pragma once\n// GENERADO por tools/gen_special.py (PokeAPI pokemon_stats.csv). No editar.\n')
        f.write('// ko12.8: ataque especial / defensa especial base de cada especie (0 = sin usar)\n')
        f.write('#include <stdint.h>\n\n')
        f.write(f'static const uint8_t DEX_SPA[{N + 1}] = {{\n{arr(spa)}\n}};\n')
        f.write(f'static const uint8_t DEX_SPD[{N + 1}] = {{\n{arr(spd)}\n}};\n')
    print(OUT)


if __name__ == '__main__':
    main()
