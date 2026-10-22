#!/usr/bin/env python3
"""Renderiza las pantallas OLED de Eolo a PNG sin hardware.

Compila src/Ui (Renderer, GUI y escenas reales) contra un Context simulado con el
compilador de `ziglang` (pip install ziglang) y la libreria U8g2 de .pio/libdeps,
ejecuta preview.cpp y convierte cada buffer de 128x64 a PNG (x4).

Uso:
    python scripts/ui_preview/build.py [--variant express|standard|dron] [--out DIR] [--scale N]

Salida (por defecto build/ui_preview/<variant>/): <pantalla>.png, sheet_header.png,
sheet_screens.png e index.txt con el orden de las hojas.
Los datos de sensores son ficticios; sirve para revisar layout, no mediciones.
"""
import argparse
import glob
import os
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HERE = os.path.dirname(os.path.abspath(__file__))

# Features y numero de motores de cada variante (ver platformio.ini).
VARIANTS = {
    "express": (["FEATURE_PLANTOWER", "FEATURE_FLOW_AFM07"], 2),
    "standard": (["FEATURE_PLANTOWER", "FEATURE_MODEM", "FEATURE_ANEMOMETER", "FEATURE_DUAL_BATTERY", "FEATURE_FLOW_AFM07"], 2),
    "dron": (["FEATURE_FLOW_AFM07"], 1),
}


def zig(*args):
    return [sys.executable, "-m", "ziglang", *args]


def find_u8g2():
    for path in glob.glob(os.path.join(ROOT, ".pio", "libdeps", "*", "U8g2", "src")):
        if os.path.isdir(os.path.join(path, "clib")):
            return path
    sys.exit("No se encontro U8g2 en .pio/libdeps; corre `pio run` una vez.")


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        sys.exit("fallo: " + " ".join(cmd[:6]) + " ...")
    return r


def compile_clib(u8g2_src, objdir):
    os.makedirs(objdir, exist_ok=True)
    objs = []
    for src in sorted(glob.glob(os.path.join(u8g2_src, "clib", "*.c"))):
        obj = os.path.join(objdir, os.path.basename(src) + ".o")
        if not os.path.exists(obj) or os.path.getmtime(obj) < os.path.getmtime(src):
            run(zig("cc", "-c", "-O1", "-w", "-o", obj, src))
        objs.append(obj)
    return objs


def write_png(path, width, height, rows):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def bin_to_pixels(path):
    data = open(path, "rb").read()
    # Formato u8g2: 8 paginas de 128 bytes; bit b del byte = fila pagina*8+b.
    return [[255 if data[(y // 8) * 128 + x] >> (y % 8) & 1 else 0 for x in range(128)] for y in range(64)]


def upscale(pixels, scale, shade=True):
    grid = shade and scale >= 3
    out = []
    for row in pixels:
        line = []
        edge = []
        for v in row:
            body = [v] * scale
            if grid:
                body[-1] = 150 if v else 10
            line.extend(body)
            edge.extend([150 if v else 10] * scale)
        out.extend([line] * (scale - 1))
        out.append(edge if grid else line)
    return out


def sheet(images, cols, gap, scale, crop=None):
    tiles = []
    for px in images:
        if crop:
            px = px[crop[0]:crop[1]]
        tiles.append(upscale(px, scale))
    th = len(tiles[0])
    tw = len(tiles[0][0])
    rows_n = (len(tiles) + cols - 1) // cols
    height = rows_n * th + (rows_n + 1) * gap
    width = cols * tw + (cols + 1) * gap
    canvas = [[90] * width for _ in range(height)]
    for i, tile in enumerate(tiles):
        r, c = divmod(i, cols)
        oy = gap + r * (th + gap)
        ox = gap + c * (tw + gap)
        for y, row in enumerate(tile):
            canvas[oy + y][ox:ox + tw] = row
    return width, height, canvas


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--variant", choices=sorted(VARIANTS), default="express")
    ap.add_argument("--out", default=None)
    ap.add_argument("--scale", type=int, default=4)
    args = ap.parse_args()

    features, motors = VARIANTS[args.variant]
    u8g2_src = find_u8g2()
    build = os.path.join(ROOT, "build", "ui_preview")
    out = args.out or os.path.join(build, args.variant)
    os.makedirs(out, exist_ok=True)

    objs = compile_clib(u8g2_src, os.path.join(build, "u8g2_obj"))
    exe = os.path.join(build, "preview_%s.exe" % args.variant)
    cmd = zig("c++", "-std=c++17", "-O1", "-w", "-o", exe,
              os.path.join(HERE, "preview.cpp"),
              "-I", os.path.join(HERE, "stubs"), "-I", HERE,
              "-I", os.path.join(ROOT, "src"),
              "-I", os.path.join(ROOT, "lib", "EoloCore", "include"),
              "-I", u8g2_src,
              "-include", os.path.join(HERE, "prelude.h"),
              "-DPREVIEW_MOTORS=%d" % motors,
              *["-D" + f for f in features], *objs)
    run(cmd)

    for old in glob.glob(os.path.join(out, "*")):
        os.remove(old)
    run([exe, out])

    names = []
    images = []
    for binpath in sorted(glob.glob(os.path.join(out, "*.bin"))):
        name = os.path.splitext(os.path.basename(binpath))[0]
        px = bin_to_pixels(binpath)
        up = upscale(px, args.scale)
        write_png(os.path.join(out, name + ".png"), 128 * args.scale, 64 * args.scale, up)
        names.append(name)
        images.append(px)
        os.remove(binpath)

    headers = [(n, p) for n, p in zip(names, images) if n.startswith("header_")]
    screens = [(n, p) for n, p in zip(names, images) if not n.startswith("header_")]
    index = []
    if headers:
        w, h, canvas = sheet([p for _, p in headers], 1, 6, args.scale, crop=(0, 14))
        write_png(os.path.join(out, "sheet_header.png"), w, h, canvas)
        index.append("sheet_header.png (1 columna, de arriba a abajo): " + ", ".join(n for n, _ in headers))
    if screens:
        w, h, canvas = sheet([p for _, p in screens], 3, 8, args.scale)
        write_png(os.path.join(out, "sheet_screens.png"), w, h, canvas)
        index.append("sheet_screens.png (3 columnas, filas de izq. a der.): " + ", ".join(n for n, _ in screens))
    with open(os.path.join(out, "index.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(index) + "\n")
    print("\n".join(index))
    print("Salida:", out)


if __name__ == "__main__":
    main()
