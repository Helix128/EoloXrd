#!/usr/bin/env python3
import argparse
import datetime as dt
import sys
import time


def parse_args():
    parser = argparse.ArgumentParser(description="Send host time to EOLO over the serial terminal.")
    parser.add_argument("port", nargs="?", help="Serial port, for example /dev/ttyUSB0 or COM3.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--wait", type=float, default=0.25, help="Delay between serial commands in seconds.")
    return parser.parse_args()


def auto_port():
    try:
        from serial.tools import list_ports
    except ImportError:
        return None

    ports = list(list_ports.comports())
    if len(ports) == 1:
        return ports[0].device
    return None


def local_utc_offset_seconds(now):
    offset = now.astimezone().utcoffset()
    if offset is None:
        return 0
    return int(offset.total_seconds())


def write_line(serial_port, line, wait_seconds):
    serial_port.write((line + "\n").encode("ascii"))
    serial_port.flush()
    time.sleep(wait_seconds)


def main():
    args = parse_args()
    port = args.port or auto_port()
    if not port:
        print("No serial port provided and auto-detection was not unique.", file=sys.stderr)
        print("Usage: python scripts/send_host_time.py /dev/ttyUSB0 --baud 115200", file=sys.stderr)
        return 2

    try:
        import serial
    except ImportError:
        print("pyserial is required. Install it with: python -m pip install pyserial", file=sys.stderr)
        return 2

    now = dt.datetime.now(dt.timezone.utc).astimezone()
    unix_utc = int(now.timestamp())
    offset_seconds = local_utc_offset_seconds(now)
    command = f"time epoch {unix_utc} {offset_seconds}"

    with serial.Serial(port, args.baud, timeout=2) as serial_port:
        time.sleep(args.wait)
        write_line(serial_port, "exit", args.wait)
        write_line(serial_port, "!", args.wait)
        write_line(serial_port, command, args.wait)
        write_line(serial_port, "exit", args.wait)

    print(f"Sent host time to {port}: {command}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
