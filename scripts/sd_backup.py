#!/usr/bin/env python3
"""
scripts/sd_backup.py - Herramienta para extraer y respaldar todos los archivos
de la tarjeta SD de un dispositivo EOLO a través de la consola serie.
"""

import argparse
import binascii
import os
import sys
import time
import zlib
from pathlib import Path

try:
    import serial
except ImportError:
    print("Error: falta el módulo 'pyserial'. Instálalo con pip install pyserial o usa el entorno de PlatformIO.", file=sys.stderr)
    sys.exit(1)


def parse_args():
    parser = argparse.ArgumentParser(description="Extraer todos los archivos de la SD de EOLO por consola serie.")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Puerto serie del dispositivo (defecto: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate (defecto: 115200)")
    parser.add_argument("--out", default="sd_backup_data", help="Directorio de destino para los archivos extraídos")
    parser.add_argument("--filter", default="", help="Filtrar archivos que contengan este texto (ej: '.csv')")
    return parser.parse_args()


class EoloSdClient:
    def __init__(self, port: str, baud: int):
        self.ser = serial.Serial(port, baud, timeout=2.0)
        time.sleep(0.5)

    def close(self):
        try:
            self.send_line("exit")
        except Exception:
            pass
        self.ser.close()

    def send_line(self, text: str):
        self.ser.write((text + "\n").encode("utf-8"))
        self.ser.flush()

    def enter_terminal(self):
        self.ser.reset_input_buffer()
        # Enviar doble retorno e ingreso a modo terminal silencioso
        self.send_line("")
        time.sleep(0.1)
        self.send_line("!")
        time.sleep(0.2)
        # Limpiar eco inicial
        self.ser.reset_input_buffer()

    def get_status(self) -> str:
        self.send_line("sd status")
        t0 = time.time()
        while time.time() - t0 < 3.0:
            line = self.ser.readline().decode("utf-8", errors="ignore").strip()
            if line.startswith("SD:"):
                return line
        return "SD: TIMEOUT"

    def list_files(self) -> list:
        self.send_line("sd ls")
        files = []
        in_list = False
        t0 = time.time()
        while time.time() - t0 < 8.0:
            line = self.ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            if line == "LIST_START":
                in_list = True
                continue
            if line == "LIST_END":
                break
            if in_list and line.startswith("FILE "):
                parts = line.split(" ", 2)
                if len(parts) == 3:
                    size = int(parts[1])
                    path = parts[2]
                    files.append((path, size))
        return files

    def download_file(self, remote_path: str, local_path: Path, expected_size: int) -> bool:
        local_path.parent.mkdir(parents=True, exist_ok=True)
        self.ser.reset_input_buffer()
        self.send_line(f"sd get {remote_path}")

        started = False
        bytes_received = 0
        expected_crc = None
        local_data = bytearray()

        t_last_data = time.time()
        while True:
            line = self.ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                if time.time() - t_last_data > 10.0:
                    print(f"\n[ERROR] Timeout esperando datos de {remote_path}")
                    return False
                continue

            t_last_data = time.time()

            if line.startswith("ERROR:"):
                print(f"\n[ERROR] El dispositivo reportó: {line}")
                return False

            if line.startswith("START "):
                parts = line.rsplit(" ", 1)
                if len(parts) == 2 and parts[1].isdigit():
                    expected_size = int(parts[1])
                started = True
                continue

            if line.startswith("BLK "):
                if not started:
                    continue
                parts = line.split(" ")
                if len(parts) == 4:
                    blk_seq = int(parts[1])
                    blk_len = int(parts[2])
                    hex_data = parts[3]
                    try:
                        raw = bytes.fromhex(hex_data)
                        if len(raw) == blk_len:
                            local_data.extend(raw)
                            bytes_received += blk_len
                            pct = (bytes_received / expected_size * 100) if expected_size > 0 else 100
                            print(f"\r  Descargando: {bytes_received}/{expected_size} B ({pct:.1f}%)", end="", flush=True)
                    except ValueError:
                        print(f"\n[ERROR] Trama hexadecimal corrupta en bloque {blk_seq}")
                        return False
                continue

            if line.startswith("END "):
                parts = line.rsplit(" ", 1)
                if len(parts) == 2:
                    expected_crc = parts[1].upper()
                break

        print()  # Nueva línea tras la barra de progreso
        calc_crc = f"{zlib.crc32(local_data) & 0xFFFFFFFF:08X}"

        if expected_crc and calc_crc != expected_crc:
            print(f"[ERROR] Discrepancia de CRC32 para {remote_path}: esp={expected_crc} calc={calc_crc}")
            return False

        if len(local_data) != expected_size:
            print(f"[ERROR] Discrepancia de tamaño para {remote_path}: esp={expected_size} obtenido={len(local_data)}")
            return False

        with open(local_path, "wb") as f:
            f.write(local_data)

        return True


def main():
    args = parse_args()
    out_dir = Path(args.out).resolve()
    print(f"=== EOLO SD Backup Tool ===")
    print(f"Puerto: {args.port} | Baud: {args.baud}")
    print(f"Destino local: {out_dir}")

    client = EoloSdClient(args.port, args.baud)
    try:
        print("\n[1/3] Conectando y entrando a consola serie...")
        client.enter_terminal()
        status = client.get_status()
        print(f"Estado de la SD: {status}")
        if "NO_CARD" in status:
            print("Error: No hay tarjeta SD detectada.")
            sys.exit(1)

        print("\n[2/3] Listando archivos...")
        all_files = client.list_files()
        if not all_files:
            print("No se encontraron archivos en la tarjeta.")
            sys.exit(0)

        if args.filter:
            all_files = [f for f in all_files if args.filter in f[0]]

        total_bytes = sum(f[1] for f in all_files)
        print(f"Archivos encontrados: {len(all_files)} (Total: {total_bytes / (1024*1024):.2f} MB)")
        for path, size in all_files:
            print(f"  - {path} ({size} B)")

        print("\n[3/3] Extrayendo archivos a PC...")
        success_count = 0
        fail_count = 0

        for idx, (rem_path, size) in enumerate(all_files, start=1):
            rel_path = rem_path.lstrip("/")
            local_target = out_dir / rel_path
            print(f"[{idx}/{len(all_files)}] {rem_path} ({size} B)")

            ok = False
            for attempt in range(1, 4):
                if client.download_file(rem_path, local_target, size):
                    ok = True
                    break
                else:
                    print(f"  Reintentando ({attempt}/3)...")
                    time.sleep(0.5)

            if ok:
                success_count += 1
            else:
                fail_count += 1

        print("\n=== Resumen de Extracción ===")
        print(f"Completados con éxito: {success_count}/{len(all_files)}")
        if fail_count > 0:
            print(f"Fallidos: {fail_count}")
        print(f"Directorio local: {out_dir}")

    finally:
        client.close()


if __name__ == "__main__":
    main()
