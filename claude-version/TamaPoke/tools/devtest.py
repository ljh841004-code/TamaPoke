#!/usr/bin/env python3
"""ko12.9.8 (+ko12.9.9): probar la placa de verdad desde el PC por USB (sin tocarla).

Necesita: pip install pyserial pillow   y la placa con ko12.9.8 o posterior conectada por USB.

  python tools/devtest.py info                 # chips, bateria, memoria, SD, WiFi, version
  python tools/devtest.py shot [foto.png]      # copia de la pantalla
  python tools/devtest.py scr                  # en que pantalla esta
  python tools/devtest.py tap X Y              # tocar (0..465)
  python tools/devtest.py swipe X0 Y0 X1 Y1 [ms]
  python tools/devtest.py hold X Y [ms]        # pulsacion larga
  python tools/devtest.py cmd "SDCHECK"        # un comando de la consola (solo los de leer)
  python tools/devtest.py tour                 # recorrido: ficha, pokedex, ajustes (fotos en devtest_out/)
  python tools/devtest.py soak [minutos]       # dejarla encendida y anotar memoria/bateria (HEALTH)
  python tools/devtest.py monkey N --i-made-a-backup   # N toques al azar (cambia la partida!)

Todo lo que se recibe se guarda en devtest_out/ (log.txt y las fotos).
Nunca envia comandos que borran o cambian la partida (WIPE, SPEC, LVL...) salvo con --force.
"""
import argparse
import os
import random
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("falta pyserial: pip install pyserial")

OUT = "devtest_out"
W = H = 466
# comandos de la consola que solo leen (los demas cambian la partida: hace falta --force)
READ_ONLY = {"HW", "HEALTH", "STATS", "NET", "SDINFO", "SDCHECK", "PAKINFO", "LS", "SCR", "TIME", "VOL", "REG", "GAL"}


def find_port():
    ports = list(list_ports.comports())
    for p in ports:  # Espressif (ESP32-S3 USB nativo)
        if p.vid == 0x303A:
            return p.device
    for p in ports:
        if any(k in (p.device or "") for k in ("usbmodem", "ttyACM", "COM")):
            return p.device
    sys.exit("no encuentro la placa: conectala por USB (cable de datos). Puertos: %s" % [p.device for p in ports])


class Board:
    def __init__(self, port=None):
        os.makedirs(OUT, exist_ok=True)
        self.log = open(os.path.join(OUT, "log.txt"), "a", encoding="utf-8")
        self.port = port or find_port()
        self.s = serial.Serial(self.port, 115200, timeout=0.5)
        time.sleep(0.3)
        self.s.reset_input_buffer()
        self.note("== conectado a %s ==" % self.port)

    def note(self, line):
        stamp = time.strftime("%H:%M:%S")
        self.log.write("%s %s\n" % (stamp, line))
        self.log.flush()

    def readline(self, timeout=5.0):
        end = time.time() + timeout
        buf = b""
        while time.time() < end:
            c = self.s.read(1)
            if not c:
                continue
            if c == b"\n":
                return buf.decode("utf-8", "replace").rstrip("\r")
            buf += c
        return None

    def cmd(self, line, timeout=8.0, force=False):
        parts = line.split()
        word = parts[0].upper() if parts else ""
        # solo de leer = el comando tal cual, sin argumentos (TIME 123 o VOL 1 2 3 SI cambian cosas)
        safe = (word in READ_ONLY and len(parts) == 1) or word in ("TAP", "SWIPE", "HOLD", "SHOT")
        if not safe and not force:
            sys.exit("'%s' cambia la partida: no se envia (usa --force si de verdad quieres)" % word)
        self.note("> " + line)
        self.s.write((line + "\n").encode())
        out = []
        end = time.time() + timeout
        while time.time() < end:
            l = self.readline(max(0.1, end - time.time()))
            if l is None:
                break
            self.note("< " + l)
            if l in ("DONE", "ERR"):
                return out, l == "DONE"
            out.append(l)
        return out, False

    def shot(self, path=None):
        self.note("> SHOT")
        self.s.write(b"SHOT\n")
        end = time.time() + 10
        hdr = None
        while time.time() < end:
            l = self.readline(2)
            if l and l.startswith("SHOT "):
                hdr = l
                break
            if l:
                self.note("< " + l)
        if not hdr:
            print("sin respuesta a SHOT")
            return None
        _, w, h, n = hdr.split()
        w, h, n = int(w), int(h), int(n)
        data = b""
        self.s.timeout = 5
        while len(data) < n:
            chunk = self.s.read(n - len(data))
            if not chunk:
                break
            data += chunk
        self.s.timeout = 0.5
        self.readline(1)
        self.readline(1)  # DONE
        if len(data) < n:
            print("foto incompleta (%d/%d bytes)" % (len(data), n))
            return None
        path = path or os.path.join(OUT, time.strftime("shot_%H%M%S.png"))
        try:
            from PIL import Image
            im = Image.new("RGB", (w, h))
            px = []
            for i in range(0, n, 2):
                v = data[i] | data[i + 1] << 8
                px.append((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
            im.putdata(px)
            im.save(path)
        except ImportError:
            path = path.rsplit(".", 1)[0] + ".raw"
            open(path, "wb").write(data)
        self.note("foto " + path)
        print("foto:", path)
        return path

    def gesture(self, line, wait=0.9):
        self.wake()
        self.cmd(line)
        time.sleep(wait)  # que la placa entregue el gesto y repinte

    def wake(self):
        # ko12.9.9: con la pantalla atenuada o apagada el primer toque solo la despierta (se traga el gesto)
        out, _ = self.cmd("SCR")
        st = dict(kv.split("=", 1) for kv in " ".join(out).split() if "=" in kv)
        if st.get("dim", "0") != "0" or st.get("off", "0") != "0":
            self.note("despertar pantalla")
            self.cmd("TAP 233 233")
            time.sleep(1.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what")
    ap.add_argument("args", nargs="*")
    ap.add_argument("--port")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--i-made-a-backup", action="store_true")
    a = ap.parse_args()
    b = Board(a.port)
    w = a.what.lower()
    if w == "info":
        for c in ("HW", "HEALTH", "STATS", "NET", "SDINFO", "SCR"):
            out, _ = b.cmd(c)
            print("\n".join(out))
    elif w == "shot":
        b.shot(a.args[0] if a.args else None)
    elif w == "scr":
        print("\n".join(b.cmd("SCR")[0]))
    elif w in ("tap", "swipe", "hold"):
        b.gesture(" ".join([w.upper()] + a.args))
        print("\n".join(b.cmd("SCR")[0]))
    elif w == "cmd":
        out, ok = b.cmd(" ".join(a.args), force=a.force)
        print("\n".join(out))
        print("DONE" if ok else "ERR / sin respuesta")
    elif w == "tour":
        steps = [
            ("main", None),
            ("card", "SWIPE 233 400 233 120 300"),
            ("card_p2", "SWIPE 380 233 80 233 300"),
            ("card_p3", "SWIPE 380 233 80 233 300"),
            ("card_close", "SWIPE 233 400 233 120 300"),  # la ficha se cierra deslizando otra vez hacia arriba
            ("dex", "SWIPE 80 233 380 233 300"),
            ("dex_p2", "SWIPE 380 233 80 233 300"),
            ("dex_close", "TAP 233 440"),
            ("settings", "SWIPE 233 120 233 400 300"),
            ("settings_back", "TAP 20 200"),
        ]
        for name, g in steps:
            if g:
                b.gesture(g, 1.2)
            else:
                b.wake()
            b.shot(os.path.join(OUT, "tour_%s.png" % name))
            print("  ", name, "|", " ".join(b.cmd("SCR")[0]))
    elif w == "soak":
        mins = float(a.args[0]) if a.args else 30
        end = time.time() + mins * 60
        while time.time() < end:
            out, _ = b.cmd("HEALTH")
            print(time.strftime("%H:%M:%S"), " ".join(out))
            time.sleep(60)
    elif w == "monkey":
        if not a.i_made_a_backup:
            sys.exit("los toques al azar cambian la partida (gastan objetos, combates...). Haz antes una copia "
                     "(ajustes > 세이브 백업 > 지금 백업) y repite con --i-made-a-backup")
        n = int(a.args[0]) if a.args else 200
        rnd = random.Random()
        for i in range(n):
            x, y = rnd.randrange(30, 436), rnd.randrange(30, 436)
            if rnd.random() < 0.8:
                b.gesture("TAP %d %d" % (x, y), 0.4)
            else:  # deslizar (sin pulsaciones largas: soltar al bicho / empezar de nuevo las piden)
                dx, dy = rnd.choice([(150, 0), (-150, 0), (0, 150), (0, -150)])
                b.gesture("SWIPE %d %d %d %d 300" % (x, y, x + dx, y + dy), 0.6)
            if i % 25 == 0:
                b.shot(os.path.join(OUT, "monkey_%04d.png" % i))
                out, _ = b.cmd("HEALTH")
                print(i, " ".join(out))
    else:
        sys.exit("no se que es '%s' (mira --help)" % a.what)


if __name__ == "__main__":
    main()
