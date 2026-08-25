#!/usr/bin/env python3
"""
EOLO Setup - Servidor Mock de Desarrollo Local
Permite probar la interfaz web completa desde tu PC o celular sin conectar al Wi-Fi del dron.
Uso: python3 scripts/dev_server.py [puerto]
"""

import http.server
import socketserver
import socket
import json
import urllib.parse
import sys
from pathlib import Path
from datetime import datetime

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
ROOT_DIR = Path(__file__).resolve().parents[1]
WEB_DIR = ROOT_DIR / "web-server"

# Estado simulado en memoria
sim_presets = [
    {
        "name": "nominal_5lpm",
        "durationSeconds": 300,
        "targetFlow": 5.0,
        "flowSectionCount": 0,
        "config": {
            "waitSeconds": 0,
            "durationSeconds": 300,
            "targetFlow": 5.0,
            "flowSectionCount": 0,
            "flowSections": []
        }
    },
    {
        "name": "escalera_2_4_6",
        "durationSeconds": 540,
        "targetFlow": 2.0,
        "flowSectionCount": 3,
        "config": {
            "waitSeconds": 60,
            "durationSeconds": 540,
            "targetFlow": 2.0,
            "flowSectionCount": 3,
            "flowSections": [
                {"durationSeconds": 180, "targetFlow": 2.0},
                {"durationSeconds": 180, "targetFlow": 4.0},
                {"durationSeconds": 180, "targetFlow": 6.0}
            ]
        }
    }
]

sim_logs = [
    {"name": "log_2026_08_21T14_30_00.csv", "size": 145820},
    {"name": "log_2026_08_21T10_15_22.csv", "size": 89400},
    {"name": "log_2026_08_20T16_45_10.csv", "size": 234100}
]

sim_debug = {
    "active": False,
    "pwm": 0,
    "pct": 0.0,
    "maxPwm": 2047,
    "flow": 5.02,
    "temp": 34.5
}

def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


class EoloDevHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_DIR), **kwargs)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = urllib.parse.parse_qs(parsed.query)

        if path == "/api/status":
            self.send_json({
                "sdReady": True,
                "sdStatus": "ok",
                "rtc": datetime.now().isoformat(),
                "staConnected": True,
                "staIp": get_local_ip(),
                "defaults": {
                    "waitSeconds": 0,
                    "durationSeconds": 300,
                    "targetFlow": 5.0,
                    "flowSectionCount": 0,
                    "flowSections": []
                },
                "switches": {
                    "waitCode": 0,
                    "durationCode": 1,
                    "wait": "Off (Setup Web)",
                    "duration": "5 min"
                }
            })
            return

        elif path == "/api/presets":
            self.send_json({"presets": sim_presets})
            return

        elif path == "/api/presets/load":
            name = query.get("name", [""])[0]
            found = next((p for p in sim_presets if p["name"] == name), None)
            if found:
                self.send_json({"ok": True, "name": found["name"], "config": found["config"]})
            else:
                self.send_json({"ok": False, "error": "not_found"}, 404)
            return

        elif path == "/api/logs":
            self.send_json({"available": True, "files": sim_logs})
            return

        elif path == "/api/logs/preview":
            filename = query.get("file", ["log.csv"])[0]
            header = "timestamp,flow_lpm,pwm,motor_temp,bme_press,bme_temp,bme_hum,battery_v"
            rows = [
                f"2026-08-21T18:00:{i:02d},5.01,745,34.2,1013.25,22.4,45.1,11.85"
                for i in range(10, 50)
            ]
            self.send_json({"header": header, "rows": rows})
            return

        elif path == "/download":
            filename = query.get("file", ["data.csv"])[0]
            csv_data = "timestamp,flow_lpm,pwm,motor_temp,bme_press,bme_temp,bme_hum,battery_v\n"
            for i in range(60):
                csv_data += f"2026-08-21T18:00:{i:02d},5.00,740,34.0,1013.20,22.5,45.0,11.80\n"
            
            self.send_response(200)
            self.send_header("Content-Type", "text/csv")
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
            self.send_header("Content-Length", str(len(csv_data)))
            self.end_headers()
            self.wfile.write(csv_data.encode("utf-8"))
            return

        elif path == "/api/debug/status":
            self.send_json({
                "debugMode": sim_debug["active"],
                "maxPwm": sim_debug["maxPwm"],
                "pwm": sim_debug["pwm"],
                "pct": sim_debug["pct"],
                "flow": {"valid": True, "lpm": sim_debug["flow"], "ageMs": 10},
                "motorTempValid": True,
                "motorTemp": sim_debug["temp"],
                "overheat": False
            })
            return

        elif path == "/logo_cmas_mini.png":
            logo_file = ROOT_DIR / "logo_cmas_mini.png"
            if logo_file.exists():
                data = logo_file.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "image/png")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return

        return super().do_GET()

    def do_POST(self):
        global sim_presets, sim_logs
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        length = int(self.headers.get("Content-Length", 0))
        body_bytes = self.rfile.read(length)
        body = urllib.parse.parse_qs(body_bytes.decode("utf-8"))

        if path == "/api/confirm":
            print("[MOCK] Sesion de captura confirmada con parametros:", body)
            self.send_json({"ok": True})
            return

        elif path == "/api/presets/save":
            name = body.get("name", [""])[0]
            if name:
                wait_sec = int(body.get("waitSeconds", [0])[0])
                dur_sec = int(body.get("durationSeconds", [300])[0])
                target_flow = float(body.get("targetFlow", [5.0])[0])
                schedule_raw = body.get("flowSchedule", ["[]"])[0]
                try:
                    sections = json.loads(schedule_raw)
                except Exception:
                    sections = []

                sim_presets.append({
                    "name": name,
                    "durationSeconds": dur_sec,
                    "targetFlow": target_flow,
                    "flowSectionCount": len(sections),
                    "config": {
                        "waitSeconds": wait_sec,
                        "durationSeconds": dur_sec,
                        "targetFlow": target_flow,
                        "flowSectionCount": len(sections),
                        "flowSections": sections
                    }
                })
                print(f"[MOCK] Preset '{name}' guardado exitosamente.")
                self.send_json({"ok": True})
            else:
                self.send_json({"ok": False, "error": "invalid_name"}, 400)
            return

        elif path == "/api/presets/delete":
            name = body.get("name", [""])[0]
            sim_presets = [p for p in sim_presets if p["name"] != name]
            print(f"[MOCK] Preset '{name}' eliminado.")
            self.send_json({"ok": True})
            return

        elif path == "/api/logs/delete":
            filename = body.get("file", [""])[0]
            sim_logs = [f for f in sim_logs if f["name"] != filename]
            print(f"[MOCK] Archivo '{filename}' eliminado.")
            self.send_json({"ok": True})
            return

        elif path == "/api/debug/enter":
            sim_debug["active"] = True
            print("[MOCK] Modo diagnostico activado.")
            self.send_json({"ok": True})
            return

        elif path == "/api/debug/pwm":
            pct = float(body.get("pct", [0.0])[0])
            sim_debug["pct"] = pct
            sim_debug["pwm"] = int((sim_debug["maxPwm"] * pct) / 100.0)
            sim_debug["flow"] = round(pct * 0.08, 2)
            print(f"[MOCK] PWM fijado a {sim_debug['pwm']} ({pct:.1f}%)")
            self.send_json({"ok": True})
            return

        elif path == "/api/motor/ignite":
            print("[MOCK] Pulso de ignicion (kick) ejecutado: PWM 1650 por 300 ms -> 0")
            sim_debug["pwm"] = 0
            sim_debug["pct"] = 0.0
            sim_debug["flow"] = 0.0
            self.send_json({"ok": True, "kickPwm": 1650, "durationMs": 300})
            return

        self.send_json({"error": "not_found"}, 404)

    def send_json(self, data, code=200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    local_ip = get_local_ip()
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("0.0.0.0", PORT), EoloDevHandler) as httpd:
        print("\n" + "=" * 60)
        print("  EOLO Dron - Servidor Mock de Desarrollo")
        print("=" * 60)
        print(f"  [PC Local]    -> http://localhost:{PORT}")
        print(f"  [Celular LAN] -> http://{local_ip}:{PORT}")
        print("=" * 60)
        print("  Presiona Ctrl+C para detener el servidor.\n")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServidor detenido.")


if __name__ == "__main__":
    main()
