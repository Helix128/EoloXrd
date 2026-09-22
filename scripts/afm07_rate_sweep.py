#!/usr/bin/env python3
"""Qualify AFM07 RS485 poll gaps without changing the host network.

Each interval is a separate compile (and, with ``--upload``, a separate
firmware load). The script talks only to the supplied HTTP base URL, captures
diagnostic snapshots as JSONL, and writes one CSV summary. It deliberately
stops on the first failed interval unless ``--continue-on-failure`` is given;
there is no automatic capture restart.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Any, Dict, Iterable, List, Optional
from urllib.error import HTTPError, URLError
from urllib.parse import urljoin
from urllib.request import Request, urlopen


DEFAULT_INTERVALS = (200, 400, 600, 800, 1000)
MAX_SUCCESS_GAP_MS = 1500
REST_TOLERANCE_MS = 5


class DeviceError(RuntimeError):
    pass


def http_json(base_url: str, path: str, method: str = "GET", timeout: float = 5.0) -> Any:
    url = urljoin(base_url.rstrip("/") + "/", path.lstrip("/"))
    request = Request(url, method=method, headers={"Cache-Control": "no-store"})
    try:
        with urlopen(request, timeout=timeout) as response:
            raw = response.read()
    except (HTTPError, URLError, TimeoutError) as exc:
        raise DeviceError(f"{method} {url}: {exc}") from exc
    try:
        return json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise DeviceError(f"{method} {url}: respuesta no es JSON válido") from exc


def wait_for_device(base_url: str, timeout: float) -> Dict[str, Any]:
    deadline = time.monotonic() + timeout
    last_error: Optional[Exception] = None
    while time.monotonic() < deadline:
        try:
            return http_json(base_url, "/api/diagnostics")
        except DeviceError as exc:
            last_error = exc
            time.sleep(0.5)
    raise DeviceError(f"dispositivo no respondió en {timeout:.1f}s: {last_error}")


def run_pio(project_dir: Path, env_name: str, gap_ms: int, upload: bool,
            clean: bool) -> None:
    pio = shutil.which("pio")
    if pio:
        command = [pio]
    else:
        command = [sys.executable, "-m", "platformio"]

    build_env = os.environ.copy()
    build_env["EOLO_AFM07_POLL_GAP_MS"] = str(gap_ms)
    if clean:
        subprocess.run(command + ["run", "-e", env_name, "-t", "clean"],
                       cwd=project_dir, env=build_env, check=True)
    target = "upload" if upload else "buildprog"
    subprocess.run(command + ["run", "-e", env_name, "-t", target],
                   cwd=project_dir, env=build_env, check=True)


def diagnostic_gate(base_url: str, timeout: float) -> Dict[str, Any]:
    pending = http_json(base_url, "/api/diagnostics/afm07", method="POST")
    if not pending.get("ok"):
        raise DeviceError(f"diagnóstico AFM07 rechazado: {pending}")

    sequence = pending.get("sequence")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status = http_json(base_url, "/api/diagnostics/afm07")
        if sequence is not None and status.get("sequence") != sequence:
            time.sleep(0.2)
            continue
        if status.get("state") in ("complete", "failed"):
            if status.get("state") != "complete" or not status.get("valueValid"):
                raise DeviceError(f"lectura AFM07 0x0004 fallida: {status}")
            if int(status.get("register0004", -1)) != 0:
                raise DeviceError(
                    "AFM07 0x0004 distinto de cero; detener barrido y revisar "
                    f"sensor/EEPROM/alimentación: {status}"
                )
            return status
        time.sleep(0.2)
    raise DeviceError("timeout esperando diagnóstico AFM07 0x0004")


def reset_statistics(base_url: str) -> None:
    response = http_json(base_url, "/api/diagnostics/reset", method="POST")
    if not response.get("ok"):
        raise DeviceError(f"no se pudieron reiniciar estadísticas: {response}")


def require_motor_off(base_url: str) -> None:
    """Refuse a qualification window that could be contaminated by actuation."""
    diagnostics = http_json(base_url, "/api/diagnostics")
    application = diagnostics.get("application") or {}
    if application.get("capturing"):
        raise DeviceError("la captura está activa; el barrido exige motor apagado")
    status = http_json(base_url, "/api/status")
    motor = status.get("motor") or {}
    if int(motor.get("pwm", 0) or 0) != 0:
        raise DeviceError(f"el motor no está apagado (pwm={motor.get('pwm')})")


def snapshot_ok(snapshot: Dict[str, Any], gap_ms: int) -> Dict[str, Any]:
    rs485 = snapshot.get("rs485") or {}
    sensors = snapshot.get("sensors") or {}
    slaves = rs485.get("slaves") or []
    afm = next((item for item in slaves if int(item.get("id", -1)) == 2), {})
    afm_flow = rs485.get("afm07") or {}
    failures = int(rs485.get("failures", 0))
    error_fields = (
        "timeouts", "crc", "malformed", "incomplete", "busBusy", "unexpected",
        "exceptions", "lateBytes", "protocolUnexpectedFrames",
    )
    afm_error_fields = (
        "timeouts", "crc", "malformed", "incomplete", "busBusy", "unexpected",
        "lateBytes", "protocolUnexpectedFrames", "deadlineMisses",
    )
    afm_exception_codes = afm.get("exceptionCodes") or {}
    afm_exception_total = sum(int(value or 0) for value in afm_exception_codes.values())
    def slave_has_no_errors(slave: Dict[str, Any]) -> bool:
        exception_codes = slave.get("exceptionCodes") or {}
        return (
            int(slave.get("failures", 0) or 0) == 0
            and all(int(slave.get(field, 0) or 0) == 0 for field in afm_error_fields)
            and sum(int(value or 0) for value in exception_codes.values()) == 0
        )

    zero_errors = (
        failures == 0
        and all(int(rs485.get(field, 0) or 0) == 0 for field in error_fields)
        and all(slave_has_no_errors(slave) for slave in slaves)
        and int(afm.get("failures", 0) or 0) == 0
        and all(int(afm.get(field, 0) or 0) == 0 for field in afm_error_fields)
        and afm_exception_total == 0
    )
    min_rest = afm.get("minActualRestMs", afm.get("minRestMs", 0))
    min_rest = int(min_rest or 0)
    max_success = int(afm.get("maxSuccessGapMs", 0) or 0)
    diagnostic = rs485.get("afm07Diagnostic") or {}
    diagnostic_clear = (
        not bool(rs485.get("afmSafetyBlocked"))
        and str(diagnostic.get("state", "")).lower() == "complete"
        and bool(diagnostic.get("valueValid"))
        and int(diagnostic.get("register0004", -1)) == 0
    )
    return {
        "zeroErrors": zero_errors,
        "afmOnline": str(afm.get("state", "")).lower() == "online",
        "afmFresh": bool(afm_flow.get("fresh")),
        "diagnosticClear": diagnostic_clear,
        "sdValid": bool(sensors.get("sdValid", (snapshot.get("sd") or {}).get("ready"))),
        "ntcValid": bool(sensors.get("ntcValid")),
        "bmeValid": bool(sensors.get("bmeValid")),
        "rtcValid": bool(sensors.get("rtcValid")),
        "afmSamples": int(afm.get("successes", 0) or 0),
        "maxSuccessGapMs": max_success,
        "maxAttemptGapMs": int(afm.get("maxAttemptGapMs", 0) or 0),
        "minRestMs": min_rest,
        "restMeetsGap": min_rest > 0 and min_rest + REST_TOLERANCE_MS >= gap_ms,
        "success": zero_errors
        and str(afm.get("state", "")).lower() == "online"
        and bool(afm_flow.get("fresh"))
        and diagnostic_clear
        and bool(sensors.get("sdValid", (snapshot.get("sd") or {}).get("ready")))
        and bool(sensors.get("ntcValid"))
        and bool(sensors.get("bmeValid"))
        and bool(sensors.get("rtcValid"))
        and int(afm.get("successes", 0) or 0) >= 2
        and max_success <= MAX_SUCCESS_GAP_MS
        and min_rest > 0
        and min_rest + REST_TOLERANCE_MS >= gap_ms,
    }


def qualify(samples: Iterable[Dict[str, Any]], gap_ms: int) -> Dict[str, Any]:
    snapshots = list(samples)
    if not snapshots:
        return {"success": False, "reason": "sin muestras"}
    checks = [snapshot_ok(item["diagnostics"], gap_ms) for item in snapshots]
    final = checks[-1]
    reasons = []
    if not all(item["zeroErrors"] for item in checks):
        reasons.append("errores RS485")
    if not all(item["afmOnline"] for item in checks):
        reasons.append("AFM07 offline/degradado")
    if not all(item["afmFresh"] for item in checks):
        reasons.append("AFM07 no fresco")
    if not all(item["diagnosticClear"] for item in checks):
        reasons.append("diagnóstico 0x0004 bloqueado")
    if not all(item["sdValid"] for item in checks):
        reasons.append("SD no disponible")
    if not all(item["ntcValid"] for item in checks):
        reasons.append("NTC inválido")
    if not all(item["bmeValid"] for item in checks):
        reasons.append("BME280 inválido")
    if not all(item["rtcValid"] for item in checks):
        reasons.append("RTC inválido")
    if final["afmSamples"] < 2:
        reasons.append("menos de dos éxitos AFM07")
    if final["maxSuccessGapMs"] > MAX_SUCCESS_GAP_MS:
        reasons.append("maxSuccessGapMs > 1500")
    if not final["restMeetsGap"]:
        reasons.append("descanso menor al gap configurado")
    if not all(item["success"] for item in checks):
        reasons.append("ventana con condición de aceptación incumplida")
    return {
        "success": not reasons,
        "reason": "; ".join(reasons) if reasons else "ok",
        "samples": len(snapshots),
        "maxSuccessGapMs": final["maxSuccessGapMs"],
        "maxAttemptGapMs": final["maxAttemptGapMs"],
        "minRestMs": final["minRestMs"],
        "transactions": int((snapshots[-1]["diagnostics"].get("rs485") or {}).get("transactions", 0)),
        "failures": int((snapshots[-1]["diagnostics"].get("rs485") or {}).get("failures", 0)),
    }


def write_interval_artifacts(directory: Path, gap_ms: int,
                             samples: List[Dict[str, Any]], result: Dict[str, Any]) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / "samples.jsonl").open("w", encoding="utf-8") as handle:
        for item in samples:
            handle.write(json.dumps(item, ensure_ascii=False, sort_keys=True) + "\n")
    payload = {"pollGapMs": gap_ms, "qualification": result, "samples": samples}
    (directory / "qualification.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def append_csv(path: Path, row: Dict[str, Any]) -> None:
    fields = [
        "pollGapMs", "success", "reason", "samples", "transactions", "failures",
        "maxSuccessGapMs", "maxAttemptGapMs", "minRestMs",
    ]
    exists = path.exists()
    with path.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        if not exists:
            writer.writeheader()
        writer.writerow({field: row.get(field, "") for field in fields})


def write_failure_artifacts(directory: Path, gap_ms: int, reason: str) -> None:
    """Persist a failed interval even when compilation or boot aborted early."""
    directory.mkdir(parents=True, exist_ok=True)
    payload = {
        "pollGapMs": gap_ms,
        "qualification": {"pollGapMs": gap_ms, "success": False, "reason": reason,
                           "samples": 0},
        "samples": [],
    }
    (directory / "qualification.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-dir", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--env", default="eolo_dron_low_power")
    parser.add_argument("--base-url", default="http://192.168.4.1",
                        help="URL LAN/AP del dispositivo; no se cambia la red del host")
    parser.add_argument("--intervals", nargs="+", type=int, default=list(DEFAULT_INTERVALS),
                        help="gaps en ms, normalmente 200 400 600 800 1000")
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/afm07-rate-sweep"))
    parser.add_argument("--stabilization-seconds", type=float, default=30.0)
    parser.add_argument("--measurement-seconds", type=float, default=180.0)
    parser.add_argument("--sample-seconds", type=float, default=1.0)
    parser.add_argument("--boot-timeout", type=float, default=45.0)
    parser.add_argument("--diagnostic-timeout", type=float, default=10.0)
    parser.add_argument("--upload", action="store_true",
                        help="carga cada firmware; omitido solo compila")
    parser.add_argument("--no-clean", action="store_true",
                        help="no limpia el entorno antes de cada compilación")
    parser.add_argument("--continue-on-failure", action="store_true",
                        help="continúa con el siguiente gap más lento tras un fallo")
    parser.add_argument("--preflight-ok", action="store_true",
                        help="confirma 9-24 V, masa/A-B/terminación y 0x0004=0")
    parser.add_argument("--compile-only", action="store_true",
                        help="solo compila cada gap; no contacta el dispositivo")
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    if any(gap < 50 or gap > 10000 for gap in args.intervals):
        raise SystemExit("cada gap debe estar entre 50 y 10000 ms")
    if args.upload and not args.preflight_ok:
        raise SystemExit("--upload requiere --preflight-ok antes de energizar/sondear")
    if not args.compile_only and not args.preflight_ok:
        raise SystemExit("el barrido requiere --preflight-ok (o use --compile-only)")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    summary_csv = args.output_dir / "summary.csv"
    results: List[Dict[str, Any]] = []

    for gap_ms in args.intervals:
        print(f"\n=== AFM07 poll gap {gap_ms} ms ===", flush=True)
        try:
            run_pio(args.project_dir, args.env, gap_ms, args.upload, not args.no_clean)
            if args.compile_only:
                result = {"success": True, "reason": "compilado; sin medición", "samples": 0,
                          "pollGapMs": gap_ms}
                write_interval_artifacts(args.output_dir / str(gap_ms), gap_ms, [], result)
                results.append(result)
                append_csv(summary_csv, result)
                continue

            wait_for_device(args.base_url, args.boot_timeout)
            diagnostic_gate(args.base_url, args.diagnostic_timeout)
            require_motor_off(args.base_url)
            print(f"estabilizando {args.stabilization_seconds:.0f}s", flush=True)
            time.sleep(args.stabilization_seconds)
            # Excluir del intervalo de calificación tanto el arranque como la
            # lectura de 0x0004 y los 30 s de estabilización.
            reset_statistics(args.base_url)

            samples: List[Dict[str, Any]] = []
            started = time.monotonic()
            while time.monotonic() - started < args.measurement_seconds:
                diagnostics = http_json(args.base_url, "/api/diagnostics")
                status = http_json(args.base_url, "/api/status")
                samples.append({"elapsedSeconds": time.monotonic() - started,
                                "diagnostics": diagnostics, "status": status})
                time.sleep(max(0.05, args.sample_seconds))
            result = qualify(samples, gap_ms)
            result["pollGapMs"] = gap_ms
            write_interval_artifacts(args.output_dir / str(gap_ms), gap_ms, samples, result)
            append_csv(summary_csv, result)
            results.append(result)
            print(f"resultado={result['success']} {result['reason']}", flush=True)
            if not result["success"] and not args.continue_on_failure:
                print("Fallo: detener; no se reinicia automáticamente.", file=sys.stderr)
                break
        except (DeviceError, subprocess.CalledProcessError) as exc:
            result = {"pollGapMs": gap_ms, "success": False, "reason": str(exc), "samples": 0}
            write_failure_artifacts(args.output_dir / str(gap_ms), gap_ms, str(exc))
            append_csv(summary_csv, result)
            results.append(result)
            print(f"Fallo en {gap_ms} ms: {exc}", file=sys.stderr)
            if not args.continue_on_failure:
                break

    passed = [index for index, item in enumerate(results)
              if item.get("success") and item.get("samples", 0) > 0]
    if passed:
        fastest = min(passed)
        candidate_index = fastest + 1
        if candidate_index < len(args.intervals):
            candidate = args.intervals[candidate_index]
            candidate_result = results[candidate_index] if candidate_index < len(results) else None
            if candidate_result and candidate_result.get("success") and candidate_result.get("samples", 0) > 0:
                print(f"Candidato de producción (un escalón más lento): {candidate} ms")
            else:
                print("BLOQUEADO: el escalón de margen aún no está aprobado; no iniciar producción.")
        else:
            print("BLOQUEADO: 1000 ms fue el único/más rápido aprobado; sin margen.")
    else:
        print("BLOQUEADO: ningún intervalo aprobado; no iniciar producción.")
    return 0 if results and all(item.get("success") for item in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
