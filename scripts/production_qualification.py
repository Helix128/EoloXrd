#!/usr/bin/env python3
"""Ejecuta la calificación completa de producción EOLO Dron por puerto serie.

El programa anfitrión nunca reintenta una calificación fallida. Cada imagen se
compila con un intervalo AFM07 fijo, se carga, estabiliza 30 segundos en
DronProdTest y se mide durante 180 segundos con el motor apagado.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


SWEEP_GAPS_MS: Tuple[int, ...] = (1000, 800, 600, 400, 200)
ENVIRONMENT = "demo_dronprodtest_dron"
PREFIXES = {
    "EOLO_QA_EVENT": "event",
    "EOLO_QA_SAMPLE": "sample",
    "EOLO_QA_RESULT": "result",
}
MAX_SUCCESS_GAP_MS = 1500
REST_TOLERANCE_MS = 5
BOOT_TIMEOUT_SECONDS = 60.0
SOAK_TIMEOUT_SECONDS = 30.0 + 120.0 + 45.0
LONG_TIMEOUT_SECONDS = 10.0 * 60.0 + 60.0


class QualificationError(RuntimeError):
    pass


class SpanishArgumentParser(argparse.ArgumentParser):
    def format_usage(self) -> str:
        return super().format_usage().replace("usage:", "uso:", 1)

    def format_help(self) -> str:
        return (super().format_help()
                .replace("usage:", "uso:", 1)
                .replace("options:", "opciones:", 1)
                .replace("show this help message and exit",
                         "muestra esta ayuda y termina"))

    def error(self, message: str) -> None:
        self.print_usage(sys.stderr)
        self.exit(2, f"error: {message}\n")


def parse_protocol_line(line: str) -> Optional[Dict[str, Any]]:
    """Parsea una línea estructurada e ignora los logs productivos normales."""
    # El reinicio del ESP32 puede anteponer bytes NUL de la UART antes del
    # primer mensaje; no forman parte de la línea estructurada.
    text = line.strip().lstrip("\x00")
    for prefix, kind in PREFIXES.items():
        marker = prefix + " "
        if not text.startswith(marker):
            continue
        payload = text[len(marker):]
        try:
            value = json.loads(payload)
        except json.JSONDecodeError as exc:
            raise QualificationError(f"{prefix} contiene JSON inválido: {exc}") from exc
        if not isinstance(value, dict):
            raise QualificationError(f"{prefix} debe contener un objeto JSON")
        value = dict(value)
        value["_kind"] = kind
        return value
    return None


def select_margin_gap(results: Sequence[Dict[str, Any]],
                      sweep_gaps: Sequence[int] = SWEEP_GAPS_MS,
                      margin_steps: int = 2) -> Optional[int]:
    """Elige un escalón ya aprobado con margen conservador sobre el más rápido."""
    approved = {int(item["gapMs"]) for item in results if item.get("approved")}
    if not approved:
        return None
    fastest = min(approved)
    try:
        fastest_index = list(sweep_gaps).index(fastest)
    except ValueError:
        return None
    if fastest_index < 1:
        return None
    target_index = max(0, fastest_index - margin_steps)
    candidate = int(sweep_gaps[target_index])
    return candidate if candidate in approved else None


def _zero(value: Any) -> bool:
    try:
        return int(value or 0) == 0
    except (TypeError, ValueError):
        return False


def sample_acceptance_reasons(sample: Dict[str, Any], motor_off: bool) -> List[str]:
    reasons: List[str] = []
    if not sample.get("flowValid") or not sample.get("flowFresh"):
        reasons.append("AFM07 inválido/no fresco")
    if not sample.get("bmeValid"):
        reasons.append("BME280 inválido")
    if not sample.get("rtcValid") or not sample.get("rtcMonotonic"):
        reasons.append("RTC inválido/no monotónico")
    if not sample.get("ntcValid"):
        reasons.append("NTC inválido")
    try:
        if float(sample.get("ntcC", 999.0)) >= 70.0:
            reasons.append("NTC >= 70 C")
    except (TypeError, ValueError):
        reasons.append("NTC no numérico")

    rs485 = sample.get("rs485") or {}
    if motor_off:
        for field in ("failures", "timeouts", "crc", "incomplete", "malformed",
                      "busBusy", "lateBytes", "unexpected", "exceptions", "deadlineMisses"):
            if not _zero(rs485.get(field)):
                reasons.append(f"RS485 {field}")
    else:
        if not _zero(rs485.get("exceptions")):
            reasons.append("RS485 exceptions")
        total_glitches = int(rs485.get("failures", 0) or 0)
        if total_glitches > 5:
            reasons.append(f"RS485 exceso de fallos: {total_glitches} > 5")

    i2c = sample.get("i2c") or {}
    for field in ("failures", "nacks", "timeouts", "shortReads", "recoveries"):
        val = int(i2c.get(field, 0) or 0)
        max_allowed = 0 if motor_off else 3
        if val > max_allowed:
            reasons.append(f"I2C {field}")
    if motor_off and any(not _zero(value) for value in (sample.get("pwm") or [])):
        reasons.append("PWM no es cero")
    return reasons


def evaluate_soak_samples(samples: Sequence[Dict[str, Any]], gap_ms: int) -> Dict[str, Any]:
    soak = [sample for sample in samples if sample.get("state") == "SOAK"]
    reasons: List[str] = []
    if len(soak) < 110:
        reasons.append(f"prueba en reposo incompleta: {len(soak)} muestras")
    for sample in soak:
        reasons.extend(sample_acceptance_reasons(sample, motor_off=True))
    if soak:
        stats = soak[-1].get("rs485") or {}
        try:
            if int(stats.get("maxSuccessGapMs", MAX_SUCCESS_GAP_MS + 1)) > MAX_SUCCESS_GAP_MS:
                reasons.append("maxSuccessGapMs > 1500")
            min_rest = int(stats.get("minRestMs", 0) or 0)
            if min_rest <= 0 or min_rest + REST_TOLERANCE_MS < gap_ms:
                reasons.append("minRestMs no respeta el intervalo")
        except (TypeError, ValueError):
            reasons.append("timing RS485 no numérico")
    return {
        "approved": not reasons,
        "reason": "; ".join(dict.fromkeys(reasons)) if reasons else "ok",
        "samples": len(soak),
        "gapMs": gap_ms,
    }


def evaluate_long_capture(samples: Sequence[Dict[str, Any]],
                          result: Dict[str, Any], reset_seen: bool = False) -> Dict[str, Any]:
    capture = [sample for sample in samples if sample.get("state") == "CAPTURE"]
    reasons: List[str] = []
    if reset_seen:
        reasons.append("reinicio/pérdida de alimentación durante captura")
    if result.get("result") != "READY_FOR_LONG_MEASUREMENTS":
        reasons.append(str(result.get("reason") or "la imagen informó NOT_READY"))
    if len(capture) < 580:
        reasons.append(f"captura incompleta: {len(capture)} muestras")
    for sample in capture:
        reasons.extend(sample_acceptance_reasons(sample, motor_off=False))

    first_in_band: Optional[float] = None
    post_band = 0
    in_band_count = 0
    consecutive = 0
    max_consecutive = 0
    for sample in capture:
        try:
            flow = float(sample.get("flow"))
            elapsed = float(sample.get("elapsedMs")) / 1000.0
        except (TypeError, ValueError):
            reasons.append("muestra de flujo no numérica")
            continue
        in_band = 4.5 <= flow <= 5.5
        if first_in_band is None and in_band:
            first_in_band = elapsed
        if first_in_band is not None:
            post_band += 1
            if in_band:
                in_band_count += 1
                consecutive = 0
            else:
                consecutive += 1
                max_consecutive = max(max_consecutive, consecutive)
    if first_in_band is None or first_in_band > 120.0:
        reasons.append("no entró a 4.5-5.5 L/min antes de 120 s")
    ratio = in_band_count / post_band if post_band else 0.0
    if ratio < 0.95:
        reasons.append(f"muestras en banda {ratio:.3f} < 0.95")
    if max_consecutive > 10:
        reasons.append("más de 10 s consecutivos fuera de banda")
    try:
        if float(result.get("volumeDifferenceL", 999.0)) > 0.1:
            reasons.append("volumen difiere de integración en más de 0.1 L")
    except (TypeError, ValueError):
        reasons.append("volumen final inválido")
    if not result.get("pwmFinalZero"):
        reasons.append("PWM final no es cero")
    if not result.get("captureFile"):
        reasons.append("falta archivo CSV productivo")
    if not result.get("csvFinalized"):
        reasons.append("falta fila Finalizado")
    if not result.get("masterIndexUpdated"):
        reasons.append("índice maestro no actualizado")
    if not result.get("csvReopenable"):
        reasons.append("CSV no reabrible")
    if not result.get("csvCadenceOk"):
        reasons.append("cadencia CSV de 10 s inválida")
    return {
        "approved": not reasons,
        "reason": "; ".join(dict.fromkeys(reasons)) if reasons else "ok",
        "samples": len(capture),
        "firstInBandSeconds": first_in_band,
        "inBandRatio": ratio,
        "maxConsecutiveOutsideBand": max_consecutive,
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_value(project_dir: Path, *args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=project_dir, text=True).strip()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    temporary.replace(path)


class ArtifactRecorder:
    SAMPLE_FIELDS = (
        "stage", "gapMs", "state", "uptimeMs", "elapsedMs", "rtcUnix",
        "flow", "flowValid", "flowFresh", "flowAgeMs", "bmeValid",
        "ntcValid", "ntcC", "capturedVolumeL", "integratedVolumeL",
        "pwm0", "pwm1", "rs485Failures", "rs485Timeouts", "rs485Crc",
        "rs485Incomplete", "rs485Malformed", "rs485BusBusy", "rs485LateBytes",
        "rs485Unexpected", "rs485Exceptions", "rs485DeadlineMisses",
        "rs485MaxSuccessGapMs", "rs485MinRestMs", "i2cFailures", "i2cNacks",
        "i2cTimeouts", "i2cShortReads", "i2cRecoveries",
    )

    def __init__(self, root: Path) -> None:
        self.root = root
        self.root.mkdir(parents=True, exist_ok=False)
        self.events_path = root / "events.jsonl"
        self.raw_path = root / "serial.log"
        self.samples_path = root / "samples.csv"
        with self.samples_path.open("w", newline="", encoding="utf-8") as handle:
            csv.DictWriter(handle, fieldnames=self.SAMPLE_FIELDS).writeheader()

    def raw(self, stage: str, gap_ms: int, line: str) -> None:
        with self.raw_path.open("a", encoding="utf-8") as handle:
            handle.write(f"[{stage} intervalo={gap_ms}] {line.rstrip()}\n")

    def message(self, stage: str, gap_ms: int, message: Dict[str, Any]) -> None:
        record = dict(message)
        record["_stage"] = stage
        record["_gapMs"] = gap_ms
        record["_hostTime"] = time.time()
        with self.events_path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")
        if message.get("_kind") == "sample":
            flattened = dict(message)
            flattened["stage"] = stage
            flattened["gapMs"] = gap_ms
            pwm = message.get("pwm") or []
            flattened["pwm0"] = pwm[0] if len(pwm) > 0 else ""
            flattened["pwm1"] = pwm[1] if len(pwm) > 1 else ""
            rs485 = message.get("rs485") or {}
            for field, output in (
                ("failures", "rs485Failures"), ("timeouts", "rs485Timeouts"),
                ("crc", "rs485Crc"), ("incomplete", "rs485Incomplete"),
                ("malformed", "rs485Malformed"), ("busBusy", "rs485BusBusy"),
                ("lateBytes", "rs485LateBytes"), ("unexpected", "rs485Unexpected"),
                ("exceptions", "rs485Exceptions"),
                ("deadlineMisses", "rs485DeadlineMisses"),
                ("maxSuccessGapMs", "rs485MaxSuccessGapMs"),
                ("minRestMs", "rs485MinRestMs"),
            ):
                flattened[output] = rs485.get(field, "")
            i2c = message.get("i2c") or {}
            for field, output in (
                ("failures", "i2cFailures"), ("nacks", "i2cNacks"),
                ("timeouts", "i2cTimeouts"), ("shortReads", "i2cShortReads"),
                ("recoveries", "i2cRecoveries"),
            ):
                flattened[output] = i2c.get(field, "")
            with self.samples_path.open("a", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=self.SAMPLE_FIELDS, extrasaction="ignore")
                writer.writerow(flattened)


class SerialSession:
    def __init__(self, port: str, recorder: ArtifactRecorder, stage: str, gap_ms: int) -> None:
        try:
            import serial
        except ImportError as exc:
            raise QualificationError("falta pyserial: python3 -m pip install pyserial") from exc
        self._serial_module = serial
        self.port = port
        self.recorder = recorder
        self.stage = stage
        self.gap_ms = gap_ms
        self.device = None

    def __enter__(self) -> "SerialSession":
        self.device = self._serial_module.Serial(self.port, 115200, timeout=0.25)
        return self

    def __exit__(self, *_args: Any) -> None:
        if self.device is not None:
            self.device.close()

    def send(self, command: str) -> None:
        assert self.device is not None
        self.device.write((command.strip() + "\n").encode("ascii"))
        self.device.flush()

    def read_message(self) -> Optional[Dict[str, Any]]:
        assert self.device is not None
        raw = self.device.readline()
        if not raw:
            return None
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        self.recorder.raw(self.stage, self.gap_ms, line)
        message = parse_protocol_line(line)
        if message is not None:
            self.recorder.message(self.stage, self.gap_ms, message)
        return message

    def wait_boot_ready(self) -> Dict[str, Any]:
        deadline = time.monotonic() + BOOT_TIMEOUT_SECONDS
        next_status = 0.0
        while time.monotonic() < deadline:
            now = time.monotonic()
            # Abrir /dev/ttyUSB0 puede reiniciar la placa y perder BOOT_READY
            # antes de que pyserial empiece a leer. STATUS confirma el mismo
            # estado sin depender de esa ventana de arranque.
            if now >= next_status:
                self.send("STATUS")
                next_status = now + 2.0
            message = self.read_message()
            if (message and message.get("_kind") == "event" and
                    (message.get("event") == "BOOT_READY" or
                     (message.get("event") == "STATUS" and message.get("state") == "IDLE"))):
                if int(message.get("pollGapMs", -1)) != self.gap_ms:
                    raise QualificationError(
                        f"la imagen informa intervalo {message.get('pollGapMs')}, esperado {self.gap_ms}"
                    )
                return message
        raise QualificationError("tiempo de espera agotado esperando BOOT_READY")

    def run_soak(self) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
        self.send("QUALIFY")
        deadline = time.monotonic() + SOAK_TIMEOUT_SECONDS
        samples: List[Dict[str, Any]] = []
        while time.monotonic() < deadline:
            message = self.read_message()
            if not message:
                continue
            if message.get("_kind") == "sample":
                samples.append(message)
            if message.get("_kind") == "result" and message.get("result") == "NOT_READY":
                return samples, message
            if message.get("_kind") == "event" and message.get("event") == "SOAK_APPROVED":
                return samples, {"result": "SOAK_APPROVED", "reason": "ok"}
            if message.get("_kind") == "event" and message.get("event") == "BOOT":
                raise QualificationError("reinicio inesperado durante la prueba en reposo")
        raise QualificationError("tiempo de espera agotado esperando la prueba en reposo")

    def run_long(self) -> Tuple[List[Dict[str, Any]], Dict[str, Any], bool]:
        self.send("START_LONG")
        deadline = time.monotonic() + LONG_TIMEOUT_SECONDS
        next_ping = time.monotonic()
        started = False
        reset_seen = False
        samples: List[Dict[str, Any]] = []
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_ping:
                self.send("PING")
                next_ping = now + 2.0
            message = self.read_message()
            if not message:
                continue
            if message.get("_kind") == "sample":
                samples.append(message)
            if message.get("_kind") == "event" and message.get("event") == "LONG_CAPTURE_STARTED":
                started = True
            if started and message.get("_kind") == "event" and message.get("event") == "BOOT":
                reset_seen = True
            if message.get("_kind") == "result":
                return samples, message, reset_seen
        raise QualificationError("tiempo de espera agotado esperando la captura larga")


def pio_command() -> List[str]:
    executable = shutil.which("pio") or shutil.which("platformio")
    if executable:
        return [executable]
    return [sys.executable, "-m", "platformio"]


def build_and_upload(project_dir: Path, environment: str, port: str, gap_ms: int) -> Path:
    build_env = os.environ.copy()
    build_env["EOLO_AFM07_POLL_GAP_MS"] = str(gap_ms)
    command = pio_command()
    subprocess.run(command + ["run", "-e", environment, "-t", "clean"],
                   cwd=project_dir, env=build_env, check=True)
    subprocess.run(command + ["run", "-e", environment, "-t", "upload",
                              "--upload-port", port],
                   cwd=project_dir, env=build_env, check=True)
    firmware = project_dir / ".pio" / "build" / environment / "firmware.bin"
    if not firmware.is_file():
        raise QualificationError(f"PlatformIO no produjo la imagen {firmware}")
    return firmware


def validate_artifact_bundle(run_dir: Path) -> Tuple[bool, List[str]]:
    missing: List[str] = []
    for filename in ("metadata.json", "events.jsonl", "samples.csv", "result.json"):
        path = run_dir / filename
        if not path.is_file() or path.stat().st_size == 0:
            missing.append(filename)
    if not missing:
        try:
            metadata = json.loads((run_dir / "metadata.json").read_text(encoding="utf-8"))
            result = json.loads((run_dir / "result.json").read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return False, ["JSON de artefactos inválido"]
        for field in ("gitSha", "binarySha256", "mac", "environment", "gapMs"):
            if metadata.get(field) in (None, ""):
                missing.append(f"metadata.{field}")
        if result.get("result") not in ("READY_FOR_LONG_MEASUREMENTS", "NOT_READY"):
            missing.append("result.result")
    return not missing, missing


def create_run_dir(root: Path) -> Path:
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    candidate = root / stamp
    suffix = 1
    while candidate.exists():
        candidate = root / f"{stamp}-{suffix}"
        suffix += 1
    return candidate


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = SpanishArgumentParser(description=__doc__, add_help=False)
    parser.add_argument("-h", "--help", action="help",
                        help="muestra esta ayuda y termina")
    parser.add_argument("--port", required=True, help="puerto serie/carga, por ejemplo /dev/ttyUSB0")
    parser.add_argument("--preflight-ok", action="store_true",
                        help="confirma alimentación, masa, A/B, terminación y montaje seguros")
    parser.add_argument("--project-dir", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--environment", default=ENVIRONMENT)
    parser.add_argument("--gap", type=int, default=None,
                        help="intervalo AFM07 específico en ms (ej. 800) para calificar directamente sin barrido previo")
    parser.add_argument("--artifact-root", type=Path,
                        default=Path("artifacts/production-qualification"))
    return parser.parse_args(argv)


def run(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    project_dir = args.project_dir.resolve()
    if not args.preflight_ok:
        raise QualificationError("se requiere --preflight-ok antes de cargar o accionar motores")

    artifact_root = args.artifact_root
    if not artifact_root.is_absolute():
        artifact_root = project_dir / artifact_root
    run_dir = create_run_dir(artifact_root)
    recorder = ArtifactRecorder(run_dir)
    metadata: Dict[str, Any] = {
        "gitSha": git_value(project_dir, "rev-parse", "HEAD"),
        "gitDirty": bool(git_value(project_dir, "status", "--porcelain")),
        "environment": args.environment,
        "port": args.port,
        "host": platform.node(),
        "hostPlatform": platform.platform(),
        "python": platform.python_version(),
        "metrologicalAccuracyValidated": False,
        "notValidated": ["batería", "vuelo", "EMI", "intemperie", "autonomía"],
        "sweepGapsMs": [args.gap] if args.gap is not None else list(SWEEP_GAPS_MS),
    }
    write_json(run_dir / "metadata.json", metadata)

    sweep_results: List[Dict[str, Any]] = []
    active_serial: Optional[SerialSession] = None
    try:
        if args.gap is not None:
            candidate_gap = args.gap
            print(f"\nCalificación directa de AFM07 intervalo {candidate_gap} ms solicitada...", flush=True)
            write_json(run_dir / "sweep.json", [{
                "approved": True,
                "reason": "direct_gap_selected",
                "gapMs": candidate_gap,
            }])
        else:
            for gap_ms in SWEEP_GAPS_MS:
                stage = f"sweep-{gap_ms}"
                print(f"\n=== DronProdTest AFM07 intervalo {gap_ms} ms ===", flush=True)
                firmware = build_and_upload(project_dir, args.environment, args.port, gap_ms)
                binary_hash = sha256_file(firmware)
                with SerialSession(args.port, recorder, stage, gap_ms) as serial_session:
                    active_serial = serial_session
                    boot = serial_session.wait_boot_ready()
                    metadata.update({
                        "gapMs": gap_ms,
                        "binarySha256": binary_hash,
                        "mac": boot.get("mac"),
                    })
                    write_json(run_dir / "metadata.json", metadata)
                    samples, firmware_result = serial_session.run_soak()
                active_serial = None
                host_result = evaluate_soak_samples(samples, gap_ms)
                if firmware_result.get("result") != "SOAK_APPROVED":
                    host_result["approved"] = False
                    host_result["reason"] = str(firmware_result.get("reason") or "la imagen informó NOT_READY")
                host_result["binarySha256"] = binary_hash
                host_result["mac"] = boot.get("mac")
                sweep_results.append(host_result)
                interval_dir = run_dir / "sweep" / str(gap_ms)
                write_json(interval_dir / "qualification.json", {
                    "host": host_result,
                    "firmware": firmware_result,
                })
                print(f"intervalo {gap_ms}: {'APROBADO' if host_result['approved'] else 'FALLÓ'} - {host_result['reason']}")
                if not host_result["approved"]:
                    break

            write_json(run_dir / "sweep.json", sweep_results)
            candidate_gap = select_margin_gap(sweep_results)
            if candidate_gap is None:
                raise QualificationError("sin escalón aprobado de margen; producción bloqueada")

        print(f"\nCandidato de producción: {candidate_gap} ms. Cargando y ejecutando prueba en reposo...", flush=True)
        firmware = build_and_upload(project_dir, args.environment, args.port, candidate_gap)
        binary_hash = sha256_file(firmware)
        with SerialSession(args.port, recorder, "candidate", candidate_gap) as serial_session:
            active_serial = serial_session
            boot = serial_session.wait_boot_ready()
            metadata.update({
                "gapMs": candidate_gap,
                "binarySha256": binary_hash,
                "mac": boot.get("mac"),
            })
            write_json(run_dir / "metadata.json", metadata)
            candidate_samples, candidate_firmware = serial_session.run_soak()
            candidate_host = evaluate_soak_samples(candidate_samples, candidate_gap)
            if candidate_firmware.get("result") != "SOAK_APPROVED" or not candidate_host["approved"]:
                raise QualificationError(
                    "repetición del candidato falló: " +
                    str(candidate_firmware.get("reason") or candidate_host["reason"])
                )

            while True:
                confirmation = input("Escriba exactamente INICIAR CAPTURA para accionar el motor: ")
                if confirmation in ("INICIAR CAPTURA", "INICIAR 15 MIN", "INICIAR 10 MIN"):
                    break
                print("Frase no coincide; no se accionó el motor.")

            long_samples, firmware_result, reset_seen = serial_session.run_long()
            host_result = evaluate_long_capture(long_samples, firmware_result, reset_seen)
        active_serial = None

        metadata.update({
            "gapMs": candidate_gap,
            "binarySha256": binary_hash,
            "mac": boot.get("mac"),
        })
        write_json(run_dir / "metadata.json", metadata)
        final_result = dict(firmware_result)
        final_result["hostAcceptance"] = host_result
        if not host_result["approved"]:
            final_result["result"] = "NOT_READY"
            final_result["reason"] = host_result["reason"]
        write_json(run_dir / "result.json", final_result)

        complete, missing = validate_artifact_bundle(run_dir)
        if not complete:
            final_result["result"] = "NOT_READY"
            final_result["reason"] = "artefactos incompletos: " + ", ".join(missing)
            write_json(run_dir / "result.json", final_result)
            raise QualificationError(final_result["reason"])
        if final_result.get("result") != "READY_FOR_LONG_MEASUREMENTS":
            raise QualificationError(str(final_result.get("reason") or "NOT_READY"))

        print(f"\nREADY_FOR_LONG_MEASUREMENTS\nArtefactos: {run_dir}")
        return 0
    except (QualificationError, subprocess.CalledProcessError, OSError, KeyboardInterrupt) as exc:
        if active_serial is not None:
            try:
                active_serial.send("ABORT")
            except Exception:
                pass
        failure = {
            "result": "NOT_READY",
            "reason": "interrumpido por operador" if isinstance(exc, KeyboardInterrupt) else str(exc),
        }
        write_json(run_dir / "result.json", failure)
        print(f"\nNOT_READY: {failure['reason']}\nArtefactos: {run_dir}", file=sys.stderr)
        return 1


def main() -> int:
    try:
        return run()
    except QualificationError as exc:
        print(f"NOT_READY: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
