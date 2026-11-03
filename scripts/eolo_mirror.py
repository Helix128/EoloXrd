#!/usr/bin/env python3
"""Espejo de la pantalla del EOLO en una ventana del PC, por el puerto serie.

El equipo corre normal (encoder y sensores reales); este visor solo dibuja el
framebuffer que el firmware manda tras recibir 'D'. Sirve cuando la OLED no
funciona. Los logs de texto del firmware se reenvian a stdout.

    pip install pyserial pygame
    python3 scripts/eolo_mirror.py [--port /dev/ttyUSB0] [--baud 115200] [--zoom 6]

Teclas: S guarda una captura PNG, Esc/Q sale. Formato de trama en
lib/EoloCore/include/Eolo/Core/Ui/DisplayMirrorFrame.h.
"""
import argparse
import glob
import sys
import threading
import time

MAGIC = bytes([0xFF, 0xE0, 0xF0, 0x0B])
DELTA_MAGIC = bytes([0xFF, 0xE0, 0xF0, 0x0C])
HEADER = 8
KEEPALIVE_S = 2.0  # el firmware se apaga solo tras 5 s sin 'D'
STALE_S = 3.0


def crc16(data, crc=0xFFFF):
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def unpack_bits(data, expected):
    out = bytearray()
    i = 0
    while i < len(data):
        control = data[i]
        i += 1
        if control < 128:
            out += data[i : i + control + 1]
            i += control + 1
        else:
            if i >= len(data):
                return None
            out += bytes([data[i]]) * (control - 126)
            i += 1
    return bytes(out) if len(out) == expected else None


class FrameParser:
    """Separa tramas de pantalla del texto de log en un flujo de bytes.

    Mantiene el ultimo cuadro completo para aplicar las tramas delta; siempre
    entrega cuadros completos.
    """

    def __init__(self):
        self._buf = bytearray()
        self._screen = None  # (ancho_tiles, alto_tiles, bytearray)

    def _decode(self, frame):
        """Trama valida (CRC ya verificado) -> (w_tiles, h_tiles, raw) o None."""
        tiles_w, tiles_h = frame[4], frame[5]
        payload = frame[HEADER:-2]
        raw_size = tiles_w * tiles_h * 8
        if not raw_size:
            return None
        if frame[:4] == DELTA_MAGIC:
            if self._screen is None or self._screen[:2] != (tiles_w, tiles_h):
                return None  # sin base: espera el proximo cuadro completo
            mask_size = (tiles_h + 7) // 8
            mask, pages = payload[:mask_size], payload[mask_size:]
            page_size = tiles_w * 8
            raw = self._screen[2]
            pos = 0
            for page in range(tiles_h):
                if mask[page // 8] >> (page % 8) & 1:
                    chunk = pages[pos : pos + page_size]
                    if len(chunk) != page_size:
                        return None
                    raw[page * page_size : (page + 1) * page_size] = chunk
                    pos += page_size
            return tiles_w, tiles_h, raw
        raw = payload if len(payload) == raw_size else unpack_bits(payload, raw_size)
        if raw is None:
            return None
        self._screen = (tiles_w, tiles_h, bytearray(raw))
        return self._screen

    def feed(self, chunk):
        """Devuelve (texto_bytes, [(ancho_px, alto_px, framebuffer), ...])."""
        self._buf += chunk
        text = bytearray()
        frames = []
        while True:
            starts = [i for i in (self._buf.find(MAGIC), self._buf.find(DELTA_MAGIC)) if i >= 0]
            start = min(starts) if starts else -1
            if start < 0:
                # Conserva un posible magic partido al final del chunk.
                keep = 0
                for n in range(min(len(MAGIC) - 1, len(self._buf)), 0, -1):
                    if self._buf.endswith(MAGIC[:n]):
                        keep = n
                        break
                text += self._buf[: len(self._buf) - keep]
                del self._buf[: len(self._buf) - keep]
                break
            text += self._buf[:start]
            del self._buf[:start]
            if len(self._buf) < HEADER:
                break
            length = self._buf[6] | (self._buf[7] << 8)
            total = HEADER + length + 2
            if len(self._buf) < total:
                break
            frame = bytes(self._buf[:total])
            crc = frame[total - 2] | (frame[total - 1] << 8)
            if crc == crc16(frame[4 : total - 2]):
                decoded = self._decode(frame)
                if decoded is not None:
                    tiles_w, tiles_h, raw = decoded
                    frames.append((tiles_w * 8, tiles_h * 8, bytes(raw)))
                del self._buf[:total]
            else:
                # Trama danada o falso magic: descarta solo el primer byte y resincroniza.
                text += self._buf[:1]
                del self._buf[:1]
        return bytes(text), frames


# Por cada bit de fila: byte del framebuffer -> 0/1 (translate corre en C).
_BIT_TABLES = [bytes((v >> bit) & 1 for v in range(256)) for bit in range(8)]


def to_pixels(width, height, raw):
    """Framebuffer U8g2 (paginas de 8 filas, bit 0 arriba) -> bytes 0/1 fila a fila."""
    rows = []
    for page in range(height // 8):
        columns = bytes(raw[page * width : (page + 1) * width])
        rows.extend(columns.translate(_BIT_TABLES[bit]) for bit in range(8))
    return b"".join(rows)


def find_port():
    ports = sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))
    return ports[0] if ports else None


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port", help="puerto serie (por defecto el primer ttyUSB/ttyACM)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--zoom", type=int, default=6)
    args = parser.parse_args()

    import pygame
    import serial

    port = args.port or find_port()
    if not port:
        sys.exit("No hay puerto serie; usa --port")
    # Timeout solo para poder cortar el hilo lector: con datos la lectura vuelve ya.
    link = serial.Serial(port, args.baud, timeout=0.1)
    link.write(b"D")

    pygame.init()
    screen = pygame.display.set_mode((128 * args.zoom, 64 * args.zoom), pygame.RESIZABLE)
    frame_event = pygame.USEREVENT + 1
    keepalive_event = pygame.USEREVENT + 2
    status_event = pygame.USEREVENT + 3
    link_lost_event = pygame.USEREVENT + 4
    on, off = (210, 235, 255), (0, 0, 0)

    latest = [None]  # ultimo cuadro sin pintar; el lector lo reemplaza (cuadros viejos se descartan)
    lock = threading.Lock()
    stop = threading.Event()

    def read_loop():
        frame_parser = FrameParser()
        while not stop.is_set():
            try:
                chunk = link.read(link.in_waiting or 1)
            except (serial.SerialException, OSError):
                pygame.event.post(pygame.event.Event(link_lost_event))
                return
            if not chunk:
                continue
            text, frames = frame_parser.feed(chunk)
            if text:
                sys.stdout.write(text.decode("utf-8", errors="replace"))
                sys.stdout.flush()
            if frames:
                with lock:
                    notify = latest[0] is None
                    latest[0] = (frames[-1], time.monotonic())
                if notify:  # un solo aviso pendiente: el hilo principal no se llena de eventos
                    pygame.event.post(pygame.event.Event(frame_event))

    reader = threading.Thread(target=read_loop, name="eolo-mirror-reader", daemon=True)
    reader.start()
    pygame.time.set_timer(keepalive_event, int(KEEPALIVE_S * 1000))
    pygame.time.set_timer(status_event, 500)

    title = ""
    surface = None
    last_frame_at = None
    shots = 0

    try:
        running = True
        while running:
            # Duerme hasta que llegue un cuadro, una tecla o un temporizador: sin sondeo.
            events = [pygame.event.wait()] + pygame.event.get()
            for event in events:
                if event.type == pygame.QUIT or (
                    event.type == pygame.KEYDOWN and event.key in (pygame.K_ESCAPE, pygame.K_q)
                ):
                    running = False
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_s and surface:
                    shots += 1
                    name = f"eolo_mirror_{shots}.png"
                    pygame.image.save(pygame.transform.scale(surface, (surface.get_width() * 4, surface.get_height() * 4)), name)
                    print(f"captura: {name}")
                elif event.type == keepalive_event:
                    link.write(b"D")
                elif event.type == link_lost_event:
                    print("puerto serie perdido", file=sys.stderr)
                    running = False
                elif event.type == frame_event:
                    with lock:
                        (width, height, raw), last_frame_at = latest[0]
                        latest[0] = None
                    indexed = pygame.image.frombuffer(
                        bytearray(to_pixels(width, height, raw)), (width, height), "P"
                    )
                    indexed.set_palette([off, on])  # antes de copy(): sin paleta falla
                    surface = indexed.copy()  # desacopla del buffer temporal

            screen.fill(off)
            if surface:
                size = screen.get_size()
                scale = max(1, min(size[0] // surface.get_width(), size[1] // surface.get_height()))
                scaled = pygame.transform.scale(
                    surface, (surface.get_width() * scale, surface.get_height() * scale)
                )
                screen.blit(scaled, ((size[0] - scaled.get_width()) // 2, (size[1] - scaled.get_height()) // 2))
            if last_frame_at is None:
                status = "esperando cuadros..."
            else:
                status = "sin senal" if time.monotonic() - last_frame_at > STALE_S else "en vivo"
            if status != title:
                title = status
                pygame.display.set_caption(f"EOLO espejo - {port} - {status}")
            pygame.display.flip()
    finally:
        stop.set()
        reader.join(0.5)
        try:
            link.write(b"d")
        finally:
            link.close()
            pygame.quit()


if __name__ == "__main__":
    main()
