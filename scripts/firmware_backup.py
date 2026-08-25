#!/usr/bin/env python3
"""
firmware_backup.py - Auto-respaldo y versionado de binarios compilados para EOLO.

Soporta:
  - Respaldo automatico de firmware.bin, bootloader.bin, partitions.bin, firmware.elf.
  - Organizacion por Dispositivos (firmware_backups/devices/) y Demos (firmware_backups/demos/).
  - Nombres legibles y metadatos detallados (build_info.json).
  - Scripts de flasheo rapido directo (flash_firmware.sh / flash_firmware.bat).
  - Carpeta 'latest/' con enlace o copia del build mas reciente.
  - Tag de sesion (.build_tag), renombrado retroactivo (name-last) y snapshots formales.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
BACKUPS_DIR = PROJECT_ROOT / "firmware_backups"
DEVICES_BACKUP_DIR = BACKUPS_DIR / "devices"
DEMOS_BACKUP_DIR = BACKUPS_DIR / "demos"
RELEASES_BACKUP_DIR = BACKUPS_DIR / "releases"
OTHER_BACKUP_DIR = BACKUPS_DIR / "other"
BUILD_TAG_FILE = PROJECT_ROOT / ".build_tag"
HISTORY_DIR_NAME = "history"
HISTORY_KEEP_COUNT = 9
XZ_PRESET = 6

# Flash offsets para ESP32 estandar
FLASH_OFFSETS = {
    "bootloader": "0x1000",
    "partitions": "0x8000",
    "firmware": "0x10000",
}


def sanitize_name(name: str) -> str:
    """Convierte un texto en un nombre seguro para archivos y carpetas."""
    if not name:
        return ""
    clean = re.sub(r"[^\w\-\.]+", "_", name.strip())
    clean = re.sub(r"_+", "_", clean).strip("_")
    # Evita que nombres formados solo por puntos se resuelvan al directorio
    # actual o a su padre, sin impedir versiones como "V1.2".
    if not clean.strip("."):
        return ""
    return clean


def get_git_info(project_dir: Path = PROJECT_ROOT) -> dict:
    """Obtiene informacion de git (commit corto, rama, estado sucio)."""
    info = {
        "commit": "unknown",
        "branch": "unknown",
        "dirty": False,
        "tag": "",
    }
    try:
        # Commit corto
        commit = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=str(project_dir),
            stderr=subprocess.DEVNULL,
        ).decode("utf-8").strip()
        info["commit"] = commit

        # Rama
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=str(project_dir),
            stderr=subprocess.DEVNULL,
        ).decode("utf-8").strip()
        info["branch"] = branch

        # Estado sucio (cambios sin commit)
        status = subprocess.check_output(
            ["git", "status", "--porcelain"],
            cwd=str(project_dir),
            stderr=subprocess.DEVNULL,
        ).decode("utf-8").strip()
        info["dirty"] = len(status) > 0

        # Tag git si existe exactamente en este commit
        try:
            tag = subprocess.check_output(
                ["git", "describe", "--tags", "--exact-match"],
                cwd=str(project_dir),
                stderr=subprocess.DEVNULL,
            ).decode("utf-8").strip()
            info["tag"] = tag
        except Exception:
            pass
    except Exception:
        pass
    return info


def get_session_tag() -> str:
    """Obtiene el nombre de version actual almacenado en .build_tag."""
    if BUILD_TAG_FILE.exists():
        try:
            content = BUILD_TAG_FILE.read_text(encoding="utf-8").strip()
            return sanitize_name(content)
        except Exception:
            return ""
    return ""


def set_session_tag(tag: str) -> None:
    """Guarda un nombre de version en .build_tag."""
    clean = sanitize_name(tag)
    if clean:
        BUILD_TAG_FILE.write_text(clean + "\n", encoding="utf-8")
        print(f"Nombre de version actual fijado a: '{clean}'")
    else:
        clear_session_tag()


def clear_session_tag() -> None:
    """Elimina el archivo .build_tag."""
    if BUILD_TAG_FILE.exists():
        BUILD_TAG_FILE.unlink()
        print("Nombre de version actual eliminado. Modo automatico activo.")
    else:
        print("No habia ningun nombre de version activo.")


def get_daily_revision(target_dir: Path, date_str: str) -> int:
    """Calcula la siguiente revision sin reiniciarla al rotar el historial."""
    max_revision = 0
    for _, meta in iter_target_backup_metadata(target_dir):
        timestamp = str(meta.get("timestamp", meta.get("created_at", "")))
        if not timestamp.startswith(date_str):
            continue
        match = re.search(r"(?:^|_)r(\d+)(?:$|_)", str(meta.get("version_tag", "")))
        if match:
            max_revision = max(max_revision, int(match.group(1)))
    return max_revision + 1


def resolve_target_classification(env_name: str, demo_name: str = "", demo_model: str = "") -> tuple:
    """
    Clasifica el objetivo de compilacion en:
      - (category, target_folder, friendly_label)
    """
    if demo_name and demo_model:
        cat = "demos"
        folder = DEMOS_BACKUP_DIR / sanitize_name(demo_name) / sanitize_name(demo_model)
        label = f"Demo_{sanitize_name(demo_name)}_{sanitize_name(demo_model)}"
        return cat, folder, label

    if env_name.startswith("demo_"):
        parts = env_name.split("_")
        if len(parts) >= 3:
            d_name = parts[1].upper()
            d_model = "_".join(parts[2:])
            cat = "demos"
            folder = DEMOS_BACKUP_DIR / d_name / d_model
            label = f"Demo_{d_name}_{d_model}"
            return cat, folder, label

    # Dispositivos EOLO principales
    if env_name.startswith("eolo_"):
        friendly = "".join(p.capitalize() for p in env_name.split("_"))
        cat = "devices"
        folder = DEVICES_BACKUP_DIR / env_name
        label = friendly
        return cat, folder, label

    # Cualquier otro entorno
    cat = "other"
    folder = OTHER_BACKUP_DIR / env_name
    label = sanitize_name(env_name).capitalize()
    return cat, folder, label


def calculate_file_hash(filepath: Path) -> str:
    """Calcula SHA-256 de un archivo."""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def _metadata_from_archive(archive_path: Path) -> dict:
    """Lee build_info.json de un .tar.xz sin extraerlo al disco."""
    with tarfile.open(archive_path, "r:xz") as archive:
        member = next((m for m in archive.getmembers() if m.name.endswith("/build_info.json")), None)
        if not member:
            raise ValueError("El archivo no contiene build_info.json")
        source = archive.extractfile(member)
        if source is None:
            raise ValueError("No se pudo leer build_info.json")
        return json.loads(source.read().decode("utf-8"))


def iter_target_backup_metadata(target_dir: Path):
    """Entrega (ruta, metadata) de Latest, históricos y snapshots legados."""
    latest = target_dir / "latest" / "build_info.json"
    if latest.exists():
        try:
            yield latest.parent, json.loads(latest.read_text(encoding="utf-8"))
        except (OSError, ValueError, json.JSONDecodeError):
            pass

    history_dir = target_dir / HISTORY_DIR_NAME
    if history_dir.exists():
        for archive in history_dir.glob("*.tar.xz"):
            try:
                yield archive, _metadata_from_archive(archive)
            except (OSError, ValueError, tarfile.TarError, json.JSONDecodeError):
                continue

    if not target_dir.exists():
        return
    for child in target_dir.iterdir():
        if not child.is_dir() or child.name in ("latest", HISTORY_DIR_NAME) or child.name.startswith("."):
            continue
        meta_file = child / "build_info.json"
        if not meta_file.exists():
            continue
        try:
            yield child, json.loads(meta_file.read_text(encoding="utf-8"))
        except (OSError, ValueError, json.JSONDecodeError):
            continue


def _backup_sort_key(path: Path, metadata: dict) -> tuple:
    timestamp = str(metadata.get("timestamp", metadata.get("created_at", "")))
    try:
        mtime = path.stat().st_mtime
    except OSError:
        mtime = 0
    return timestamp, mtime


def _archive_directory(source_dir: Path, history_dir: Path, snapshot_name: str) -> Path:
    """Comprime un respaldo, lo verifica y recién entonces devuelve su archivo final."""
    archive_name = f"{sanitize_name(snapshot_name) or 'backup'}.tar.xz"
    archive_path = history_dir / archive_name
    partial_path = history_dir / f".{archive_name}.partial"
    history_dir.mkdir(parents=True, exist_ok=True)
    if partial_path.exists():
        partial_path.unlink()

    with tarfile.open(partial_path, "w:xz", preset=XZ_PRESET) as archive:
        archive.add(source_dir, arcname=archive_name[:-7])

    try:
        _metadata_from_archive(partial_path)
        with tarfile.open(partial_path, "r:xz") as archive:
            names = {member.name for member in archive.getmembers() if member.isfile()}
            root = archive_name[:-7]
            if f"{root}/build_info.json" not in names:
                raise ValueError("El archivo comprimido no conserva build_info.json")
    except Exception:
        partial_path.unlink(missing_ok=True)
        raise

    partial_path.replace(archive_path)
    return archive_path


def _archive_latest_if_needed(target_folder: Path) -> None:
    """Archiva el Latest previo solo si ya pertenece a esta política."""
    latest_dir = target_folder / "latest"
    meta_file = latest_dir / "build_info.json"
    if not meta_file.exists():
        return
    metadata = json.loads(meta_file.read_text(encoding="utf-8"))
    if not metadata.get("compressed_history_managed", False):
        return
    source_name = str(metadata.get("source_snapshot", ""))
    _archive_directory(latest_dir, target_folder / HISTORY_DIR_NAME,
                       source_name or f"latest_{metadata.get('timestamp', 'unknown')}")


def rotate_target_backups(target_folder: Path, keep_count: int = HISTORY_KEEP_COUNT,
                          current_snapshot_name: str = "") -> dict:
    """Conserva hasta N archivos históricos creados por esta política."""
    keep_count = max(0, min(keep_count, HISTORY_KEEP_COUNT))
    history_dir = target_folder / HISTORY_DIR_NAME
    archives = []
    if history_dir.exists():
        for archive in history_dir.glob("*.tar.xz"):
            try:
                archives.append((archive, _metadata_from_archive(archive)))
            except (OSError, ValueError, tarfile.TarError, json.JSONDecodeError):
                continue
    archives.sort(key=lambda item: _backup_sort_key(*item), reverse=True)
    deleted = 0
    for archive, _ in archives[keep_count:]:
        archive.unlink(missing_ok=True)
        deleted += 1

    if history_dir.exists() and not any(history_dir.iterdir()):
        history_dir.rmdir()

    return {"archived": 0, "deleted": deleted, "kept": min(len(archives), keep_count)}


def discover_auto_backup_targets():
    """Encuentra targets administrados, sin atravesar releases formales."""
    targets = set()
    for root in (DEVICES_BACKUP_DIR, DEMOS_BACKUP_DIR, OTHER_BACKUP_DIR):
        if not root.exists():
            continue
        for meta_file in root.rglob("build_info.json"):
            parent = meta_file.parent
            if parent.name == "latest":
                targets.add(parent.parent)
            elif HISTORY_DIR_NAME not in parent.parts:
                targets.add(parent.parent)
    return sorted(targets)


def generate_flashing_scripts(dest_dir: Path, firmware_name: str, bootloader_name: str, partitions_name: str, label: str) -> None:
    """Genera scripts ejecutables flash_firmware.sh y flash_firmware.bat."""
    sh_content = f"""#!/usr/bin/env bash
# Script de flasheo rapido para {label}
set -e

PORT="${{1:-/dev/ttyUSB0}}"
BAUD="${{2:-921600}}"

SCRIPT_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"

echo "=================================================="
echo " Flasheando {label}"
echo " Puerto: $PORT | Baudrate: $BAUD"
echo "=================================================="

ESPTOOL_CMD="esptool.py"
if ! command -v esptool.py &> /dev/null; then
    if python3 -m esptool version &> /dev/null; then
        ESPTOOL_CMD="python3 -m esptool"
    elif command -v pio &> /dev/null; then
        ESPTOOL_CMD="pio pkg exec -p espressif32 -- esptool.py"
    else
        echo "Error: no se encontro esptool.py ni PlatformIO."
        exit 1
    fi
fi

$ESPTOOL_CMD --chip esp32 --port "$PORT" --baud "$BAUD" \\
    --before default_reset --after hard_reset write_flash -z \\
    --flash_mode qio --flash_freq 80m --flash_size 4MB \\
    {FLASH_OFFSETS['bootloader']} "$SCRIPT_DIR/{bootloader_name}" \\
    {FLASH_OFFSETS['partitions']} "$SCRIPT_DIR/{partitions_name}" \\
    {FLASH_OFFSETS['firmware']} "$SCRIPT_DIR/{firmware_name}"

echo "=================================================="
echo " Flasheo completado exitosamente."
echo "=================================================="
"""
    sh_file = dest_dir / "flash_firmware.sh"
    sh_file.write_text(sh_content, encoding="utf-8")
    try:
        sh_file.chmod(0o755)
    except Exception:
        pass

    bat_content = f"""@echo off
rem Script de flasheo rapido para {label} en Windows
set PORT=%~1
if "%PORT%"=="" set PORT=COM3
set BAUD=%~2
if "%BAUD%"=="" set BAUD=921600

echo ==================================================
echo  Flasheando {label}
echo  Puerto: %PORT% ^| Baudrate: %BAUD%
echo ==================================================

esptool.py --chip esp32 --port %PORT% --baud %BAUD% ^
    --before default_reset --after hard_reset write_flash -z ^
    --flash_mode qio --flash_freq 80m --flash_size 4MB ^
    {FLASH_OFFSETS['bootloader']} "%~dp0{bootloader_name}" ^
    {FLASH_OFFSETS['partitions']} "%~dp0{partitions_name}" ^
    {FLASH_OFFSETS['firmware']} "%~dp0{firmware_name}"

if %ERRORLEVEL% equ 0 (
    echo ==================================================
    echo  Flasheo completado exitosamente.
    echo ==================================================
) else (
    echo Error durante el flasheo.
)
"""
    bat_file = dest_dir / "flash_firmware.bat"
    bat_file.write_text(bat_content, encoding="utf-8")


def create_backup(build_dir: Path, env_name: str, project_dir: Path = PROJECT_ROOT,
                  custom_tag: str = "", demo_name: str = "", demo_model: str = "") -> Path:
    """
    Ejecuta el respaldo de los binarios desde build_dir hacia firmware_backups/.
    """
    firmware_src = build_dir / "firmware.bin"
    if not firmware_src.exists():
        raise FileNotFoundError(f"No se encontro firmware.bin en {build_dir}")

    # Prioridad de tags: custom_tag -> variable EOLO_TAG -> archivo .build_tag -> auto
    tag = custom_tag or os.environ.get("EOLO_BUILD_TAG") or os.environ.get("EOLO_TAG") or get_session_tag()
    git_info = get_git_info(project_dir)

    now = datetime.datetime.now()
    date_str = now.strftime("%Y-%m-%d")
    time_str = now.strftime("%H-%M-%S")
    timestamp_prefix = f"{date_str}_{time_str}"

    cat, target_folder, friendly_label = resolve_target_classification(env_name, demo_name, demo_model)
    target_folder.mkdir(parents=True, exist_ok=True)

    if not tag:
        rev_num = get_daily_revision(target_folder, date_str)
        branch_clean = sanitize_name(git_info["branch"]) if git_info["branch"] not in ("main", "master", "unknown") else ""
        commit_tag = f"g{git_info['commit']}" if git_info['commit'] != "unknown" else ""
        dirty_tag = "-dirty" if git_info.get("dirty") else ""
        
        parts = []
        if branch_clean:
            parts.append(branch_clean)
        if commit_tag:
            parts.append(f"{commit_tag}{dirty_tag}")
        parts.append(f"r{rev_num}")
        version_tag = "_".join(parts)
    else:
        version_tag = sanitize_name(tag)

    snapshot_dir_name = f"{version_tag}_{timestamp_prefix}"
    snapshot_dir = target_folder / snapshot_dir_name
    snapshot_dir.mkdir(parents=True, exist_ok=True)

    fw_dst_name = "firmware.bin"
    bl_dst_name = "bootloader.bin"
    pt_dst_name = "partitions.bin"
    elf_dst_name = "firmware.elf"
    map_dst_name = "firmware.map"

    files_info = {}

    fw_dst = snapshot_dir / fw_dst_name
    shutil.copy2(firmware_src, fw_dst)
    files_info["firmware"] = {
        "file": fw_dst_name,
        "size_bytes": fw_dst.stat().st_size,
        "sha256": calculate_file_hash(fw_dst),
        "flash_offset": FLASH_OFFSETS["firmware"],
    }

    bl_src = build_dir / "bootloader.bin"
    if bl_src.exists():
        bl_dst = snapshot_dir / bl_dst_name
        shutil.copy2(bl_src, bl_dst)
        files_info["bootloader"] = {
            "file": bl_dst_name,
            "size_bytes": bl_dst.stat().st_size,
            "sha256": calculate_file_hash(bl_dst),
            "flash_offset": FLASH_OFFSETS["bootloader"],
        }
    else:
        bl_dst_name = "bootloader.bin"

    pt_src = build_dir / "partitions.bin"
    if pt_src.exists():
        pt_dst = snapshot_dir / pt_dst_name
        shutil.copy2(pt_src, pt_dst)
        files_info["partitions"] = {
            "file": pt_dst_name,
            "size_bytes": pt_dst.stat().st_size,
            "sha256": calculate_file_hash(pt_dst),
            "flash_offset": FLASH_OFFSETS["partitions"],
        }
    else:
        pt_dst_name = "partitions.bin"

    elf_src = build_dir / "firmware.elf"
    if elf_src.exists():
        elf_dst = snapshot_dir / elf_dst_name
        shutil.copy2(elf_src, elf_dst)
        files_info["elf"] = {
            "file": elf_dst_name,
            "size_bytes": elf_dst.stat().st_size,
            "sha256": calculate_file_hash(elf_dst),
        }

    map_src = build_dir / "firmware.map"
    if map_src.exists():
        map_dst = snapshot_dir / map_dst_name
        shutil.copy2(map_src, map_dst)
        files_info["map"] = {
            "file": map_dst_name,
            "size_bytes": map_dst.stat().st_size,
            "sha256": calculate_file_hash(map_dst),
        }

    generate_flashing_scripts(snapshot_dir, fw_dst_name, bl_dst_name, pt_dst_name, friendly_label)

    metadata = {
        "target": env_name,
        "label": friendly_label,
        "category": cat,
        "version_tag": version_tag,
        "timestamp": now.isoformat(),
        "created_at": now.strftime("%Y-%m-%d %H:%M:%S"),
        "git": git_info,
        "files": files_info,
        "flash_offsets": FLASH_OFFSETS,
    }
    meta_file = snapshot_dir / "build_info.json"
    meta_file.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")

    # El Latest previo se archiva solo si fue creado por esta política. Así no
    # se convierte en histórico ningún respaldo anterior a su activación.
    _archive_latest_if_needed(target_folder)
    update_latest_folder(target_folder, snapshot_dir, friendly_label, metadata, fw_dst_name, bl_dst_name, pt_dst_name)
    rotation = rotate_target_backups(target_folder, current_snapshot_name=snapshot_dir.name)
    shutil.rmtree(snapshot_dir)

    fw_size_kb = files_info["firmware"]["size_bytes"] / 1024.0
    latest_dir = target_folder / "latest"
    rel_path = latest_dir.relative_to(project_dir)
    print("\n" + "=" * 60)
    print(f" [RESPALDO GUARDADO] {friendly_label}")
    print(f"  Version/Tag : {version_tag}")
    print(f"  Tamano      : {fw_size_kb:.1f} KB")
    print(f"  Ubicacion   : {rel_path}")
    print(f"  Historial   : {rotation['kept']} comprimidos (XZ-{XZ_PRESET})")
    print(f"  Flasheo     : {rel_path / 'flash_firmware.sh'}")
    print("=" * 60 + "\n")

    return latest_dir


def update_latest_folder(target_folder: Path, snapshot_dir: Path, friendly_label: str, metadata: dict,
                         fw_name: str, bl_name: str, pt_name: str) -> None:
    """Mantiene una carpeta latest/ actualizada con la version mas reciente."""
    latest_dir = target_folder / "latest"
    staging_dir = target_folder / ".latest.new"
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)

    latest_fw_name = f"{friendly_label}_latest_firmware.bin"
    latest_bl_name = f"{friendly_label}_latest_bootloader.bin"
    latest_pt_name = f"{friendly_label}_latest_partitions.bin"

    if (snapshot_dir / fw_name).exists():
        shutil.copy2(snapshot_dir / fw_name, staging_dir / latest_fw_name)
    if (snapshot_dir / bl_name).exists():
        shutil.copy2(snapshot_dir / bl_name, staging_dir / latest_bl_name)
    if (snapshot_dir / pt_name).exists():
        shutil.copy2(snapshot_dir / pt_name, staging_dir / latest_pt_name)

    generate_flashing_scripts(staging_dir, latest_fw_name, latest_bl_name, latest_pt_name, f"{friendly_label} (Latest)")

    latest_meta = json.loads(json.dumps(metadata))
    latest_meta["files"] = {
        key: dict(value) for key, value in metadata.get("files", {}).items()
        if key in ("firmware", "bootloader", "partitions")
    }
    for key, filename in (("firmware", latest_fw_name), ("bootloader", latest_bl_name), ("partitions", latest_pt_name)):
        if key in latest_meta["files"]:
            latest_meta["files"][key]["file"] = filename
    latest_meta["is_latest"] = True
    latest_meta["compressed_history_managed"] = True
    latest_meta["source_snapshot"] = snapshot_dir.name
    (staging_dir / "build_info.json").write_text(json.dumps(latest_meta, indent=2, ensure_ascii=False), encoding="utf-8")

    previous_dir = target_folder / ".latest.previous"
    if previous_dir.exists():
        shutil.rmtree(previous_dir)
    if latest_dir.exists():
        latest_dir.replace(previous_dir)
    try:
        staging_dir.replace(latest_dir)
    except Exception:
        if previous_dir.exists():
            previous_dir.replace(latest_dir)
        raise
    shutil.rmtree(previous_dir, ignore_errors=True)


def name_last_backup(new_name: str, target_env: str = "") -> bool:
    """Renombra o re-etiqueta el ultimo respaldo generado."""
    clean_name = sanitize_name(new_name)
    if not clean_name:
        print("Error: debes proporcionar un nombre valido.")
        return False

    # Con la rotación actual el último respaldo persistente es Latest. Se
    # actualiza su identidad para que el próximo build lo archive con este tag.
    latest_candidates = []
    if target_env:
        _, target_folder, _ = resolve_target_classification(target_env)
        latest_candidates = [target_folder / "latest"]
    else:
        latest_candidates = [target / "latest" for target in discover_auto_backup_targets()]
    current = []
    for latest_dir in latest_candidates:
        meta_file = latest_dir / "build_info.json"
        if not meta_file.exists():
            continue
        try:
            current.append((meta_file.stat().st_mtime, latest_dir, json.loads(meta_file.read_text(encoding="utf-8"))))
        except (OSError, ValueError, json.JSONDecodeError):
            continue
    if current:
        _, latest_dir, meta = max(current, key=lambda item: item[0])
        timestamp = str(meta.get("timestamp", datetime.datetime.now().isoformat()))
        try:
            suffix = datetime.datetime.fromisoformat(timestamp).strftime("%Y-%m-%d_%H-%M-%S")
        except ValueError:
            suffix = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        meta["version_tag"] = clean_name
        meta["source_snapshot"] = f"{clean_name}_{suffix}"
        meta["renamed_at"] = datetime.datetime.now().isoformat()
        (latest_dir / "build_info.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"\n[OK] Latest renombrado exitosamente a '{clean_name}'.\n")
        return True

    all_snapshots = []
    for root, dirs, files in os.walk(BACKUPS_DIR):
        root_path = Path(root)
        if root_path.name == "latest" or not (root_path / "build_info.json").exists():
            continue
        meta_file = root_path / "build_info.json"
        try:
            meta = json.loads(meta_file.read_text(encoding="utf-8"))
            if target_env and meta.get("target") != target_env:
                continue
            mtime = meta_file.stat().st_mtime
            all_snapshots.append((mtime, root_path, meta))
        except Exception:
            continue

    if not all_snapshots:
        print("No se encontraron respaldos previos para renombrar.")
        return False

    all_snapshots.sort(key=lambda x: x[0], reverse=True)
    _, last_dir, meta = all_snapshots[0]

    parent_dir = last_dir.parent
    friendly_label = meta.get("label", "Firmware")
    
    # Formato historico: fecha_hora_nombre. Formato actual: nombre_fecha_hora.
    old_match = re.match(
        r"^(\d{4}-\d{2}-\d{2})_(\d{2}-\d{2}-\d{2})(?:_.+)?$",
        last_dir.name,
    )
    new_match = re.match(
        r"^.+_(\d{4}-\d{2}-\d{2})_(\d{2}-\d{2}-\d{2})$",
        last_dir.name,
    )
    match = old_match or new_match
    if match:
        timestamp_prefix = f"{match.group(1)}_{match.group(2)}"
    else:
        timestamp_prefix = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    new_folder_name = f"{clean_name}_{timestamp_prefix}"
    new_dir = parent_dir / new_folder_name

    if new_dir == last_dir:
        print(f"El respaldo ya tiene el nombre: {clean_name}")
        return True

    last_dir.rename(new_dir)

    new_fw_name = "firmware.bin"
    new_bl_name = "bootloader.bin"
    new_pt_name = "partitions.bin"

    artifact_names = {
        "_firmware.bin": new_fw_name,
        "_bootloader.bin": new_bl_name,
        "_partitions.bin": new_pt_name,
        "_firmware.elf": "firmware.elf",
        "_firmware.map": "firmware.map",
    }
    for file_path in list(new_dir.iterdir()):
        if not file_path.is_file():
            continue
        fname = file_path.name
        for suffix, generic_name in artifact_names.items():
            if fname.endswith(suffix) and fname != generic_name:
                file_path.rename(new_dir / generic_name)
                break

    meta["version_tag"] = clean_name
    meta["renamed_at"] = datetime.datetime.now().isoformat()
    for key, generic_name in {
        "firmware": new_fw_name,
        "bootloader": new_bl_name,
        "partitions": new_pt_name,
        "elf": "firmware.elf",
        "map": "firmware.map",
    }.items():
        if key in meta.get("files", {}):
            meta["files"][key]["file"] = generic_name
    (new_dir / "build_info.json").write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")

    generate_flashing_scripts(new_dir, new_fw_name, new_bl_name, new_pt_name, friendly_label)
    update_latest_folder(parent_dir, new_dir, friendly_label, meta, new_fw_name, new_bl_name, new_pt_name)

    print(f"\n[OK] Ultimo respaldo renombrado exitosamente a:")
    print(f"  Carpeta: {new_dir.relative_to(PROJECT_ROOT)}")
    print(f"  Tag    : {clean_name}\n")
    return True


def create_snapshot_release(target_env: str, release_name: str, notes: str = "") -> Path:
    """Crea una version formal de entrega en firmware_backups/releases/."""
    clean_name = sanitize_name(release_name)
    if not clean_name:
        raise ValueError("Nombre de snapshot invalido.")

    _, target_folder, friendly_label = resolve_target_classification(target_env)
    latest_dir = target_folder / "latest"
    if not latest_dir.exists() or not (latest_dir / "build_info.json").exists():
        print(f"No hay build previo para {target_env}. Intentando compilar primero...")
        build_and_backup(target_env)

    if not latest_dir.exists():
        raise RuntimeError(f"No se encontro compilacion previa para {target_env}")

    now = datetime.datetime.now()
    timestamp_prefix = now.strftime("%Y-%m-%d")
    release_dir_name = f"{timestamp_prefix}_{clean_name}_{friendly_label}"
    dest_dir = RELEASES_BACKUP_DIR / release_dir_name
    dest_dir.mkdir(parents=True, exist_ok=True)

    for f in latest_dir.iterdir():
        if f.is_file():
            shutil.copy2(f, dest_dir / f.name)

    readme_content = f"""# Release: {release_name}

- **Target**: {target_env} ({friendly_label})
- **Fecha**: {now.strftime("%Y-%m-%d %H:%M:%S")}
- **Notas de Version**:
{notes if notes else 'Sin notas adicionales.'}

## Instrucciones de Flasheo:
Ejecuta `./flash_firmware.sh /dev/ttyUSB0` (Linux/Mac) o `flash_firmware.bat COM3` (Windows).
"""
    (dest_dir / "README_RELEASE.md").write_text(readme_content, encoding="utf-8")

    print(f"\n[SNAPSHOT CREADO] Version formal guardada en:")
    print(f"  {dest_dir.relative_to(PROJECT_ROOT)}\n")
    return dest_dir


def list_all_backups() -> None:
    """Muestra una lista formateada de todos los respaldos existentes."""
    if not BACKUPS_DIR.exists():
        print("\nNo hay respaldos guardados aun en firmware_backups/.\n")
        return

    backups = []
    for target_folder in discover_auto_backup_targets():
        for path, meta in iter_target_backup_metadata(target_folder):
            fw_size = meta.get("files", {}).get("firmware", {}).get("size_bytes", 0)
            state = "Latest" if path.name == "latest" else "XZ"
            backups.append({
                "target": meta.get("target", "unknown"),
                "label": meta.get("label", "unknown"),
                "tag": meta.get("version_tag", ""),
                "date": meta.get("created_at", meta.get("timestamp", ""))[:19],
                "size_kb": f"{fw_size / 1024.0:.1f} KB" if fw_size else "-",
                "state": state,
            })

    if not backups:
        print("\nNo se encontraron respaldos validos en firmware_backups/.\n")
        return

    backups.sort(key=lambda x: x["date"], reverse=True)

    print("\n" + "=" * 90)
    print(f"{'FECHA':<20} | {'OBJETIVO / DEMO':<24} | {'TAG / VERSION':<22} | {'ESTADO':<7} | {'TAMANO':<10}")
    print("-" * 90)
    for b in backups:
        print(f"{b['date']:<20} | {b['label']:<24} | {b['tag']:<22} | {b['state']:<7} | {b['size_kb']:<10}")
    print("=" * 90)
    
    current_tag = get_session_tag()
    if current_tag:
        print(f"(*) Nombre de version actual activo: '{current_tag}'")
    print("")


def prune_old_backups(keep_count: int = HISTORY_KEEP_COUNT) -> None:
    """Recorta únicamente históricos comprimidos creados por esta política."""
    if not 0 <= keep_count <= HISTORY_KEEP_COUNT:
        raise ValueError(f"--keep debe estar entre 0 y {HISTORY_KEEP_COUNT}")
    totals = {"archived": 0, "deleted": 0}
    for target_folder in discover_auto_backup_targets():
        result = rotate_target_backups(target_folder, keep_count)
        totals["archived"] += result["archived"]
        totals["deleted"] += result["deleted"]
    print(f"Limpieza completada: {totals['deleted']} históricos eliminados (se conservan {keep_count} por target).")


def build_and_backup(target: str, tag: str = "") -> int:
    """Compila un target usando PlatformIO y asegura el respaldo."""
    env_name = target
    if not env_name.startswith("eolo_") and not env_name.startswith("demo_"):
        candidate = f"eolo_{target}"
        env_name = candidate

    pio_cmd = shutil.which("pio") or shutil.which("platformio")
    if not pio_cmd:
        print("Error: no se encontro PlatformIO ('pio' o 'platformio' en el PATH).")
        return 1

    env_vars = os.environ.copy()
    if tag:
        env_vars["EOLO_TAG"] = sanitize_name(tag)

    print(f"Compilando entorno '{env_name}'...")
    ret = subprocess.call([pio_cmd, "run", "-e", env_name], env=env_vars, cwd=str(PROJECT_ROOT))
    if ret != 0:
        print(f"Error durante la compilacion de {env_name}.")
        return ret
    return 0


def flash_target(target: str, port: str = "") -> int:
    """Ejecuta el script de flasheo del ultimo respaldo del target."""
    env_name = target
    if not env_name.startswith("eolo_") and not env_name.startswith("demo_"):
        env_name = f"eolo_{target}"

    _, target_folder, friendly_label = resolve_target_classification(env_name)
    flash_script = target_folder / "latest" / "flash_firmware.sh"
    if not flash_script.exists():
        print(f"No se encontro script de flasheo para {env_name}. Compilando primero...")
        build_res = build_and_backup(env_name)
        if build_res != 0:
            return build_res

    args = [str(flash_script)]
    if port:
        args.append(port)

    print(f"Flasheando {friendly_label} desde {flash_script.parent.relative_to(PROJECT_ROOT)}...")
    return subprocess.call(args)


def main():
    parser = argparse.ArgumentParser(
        description="Gestor de auto-respaldos y versionado de binarios para EOLO.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command")

    tag_p = sub.add_parser("tag", help="Consultar o definir el nombre de versión actual.")
    tag_p.add_argument("name", nargs="?", default="", help="Nombre de version a fijar (dejar vacio para consultar).")

    sub.add_parser("untag", help="Limpiar el nombre de versión actual activo.")
    sub.add_parser("clear-tag", help="Limpiar el nombre de versión actual activo.")

    name_last_p = sub.add_parser("name-last", help="Renombrar/etiquetar el último respaldo generado.")
    name_last_p.add_argument("name", help="Nuevo nombre o tag para el ultimo respaldo.")
    name_last_p.add_argument("--target", default="", help="Filtrar por target especifico si es necesario.")

    sub.add_parser("backups", help="Listar todos los respaldos organizados.")
    sub.add_parser("list", help="Listar todos los respaldos organizados.")

    snap_p = sub.add_parser("snapshot", help="Crear un snapshot formal de entrega.")
    snap_p.add_argument("target", help="Target a respaldar (ej. eolo_express, eolo_dron).")
    snap_p.add_argument("-n", "--name", required=True, help="Nombre del snapshot (ej. V1.2_Produccion).")
    snap_p.add_argument("--notes", default="", help="Notas o descripcion del snapshot.")

    backup_p = sub.add_parser("backup", help="Respaldar manualmente el directorio .pio/build/<env> actual.")
    backup_p.add_argument("env", help="Nombre del entorno (ej. eolo_express).")
    backup_p.add_argument("--tag", default=None, help="Tag opcional para el respaldo.")

    build_p = sub.add_parser("build", help="Compilar un dispositivo y respaldar automaticamente.")
    build_p.add_argument("target", help="Target a compilar (ej. express, dron, standard, eolo_express).")
    build_p.add_argument("--tag", default=None, help="Tag opcional para esta compilacion.")

    flash_p = sub.add_parser("flash", help="Flashear el ultimo respaldo de un target.")
    flash_p.add_argument("target", help="Target a flashear (ej. express, dron, standard).")
    flash_p.add_argument("--port", default="", help="Puerto serial (ej. /dev/ttyUSB0 o COM3).")

    prune_p = sub.add_parser("prune", help="Recortar históricos comprimidos sin tocar respaldos legados.")
    prune_p.add_argument("--keep", type=int, default=HISTORY_KEEP_COUNT,
                         help="Históricos comprimidos a conservar (0 a 9; default: 9).")

    args = parser.parse_args()

    if not args.command or args.command in ("backups", "list"):
        list_all_backups()
        return 0

    if args.command == "tag":
        if args.name:
            set_session_tag(args.name)
        else:
            cur = get_session_tag()
            if cur:
                print(f"Nombre de versión actual: '{cur}'")
            else:
                print("No hay nombre de versión activo (modo automático por fecha y commit).")
        return 0

    if args.command in ("untag", "clear-tag"):
        clear_session_tag()
        return 0

    if args.command == "name-last":
        success = name_last_backup(args.name, args.target)
        return 0 if success else 1

    if args.command == "snapshot":
        create_snapshot_release(args.target, args.name, args.notes)
        return 0

    if args.command == "backup":
        if args.tag is not None and not sanitize_name(args.tag):
            parser.error("--tag debe contener un nombre valido; no puede estar vacio.")
        build_dir = PROJECT_ROOT / ".pio" / "build" / args.env
        create_backup(build_dir, args.env, PROJECT_ROOT, custom_tag=args.tag or "")
        return 0

    if args.command == "build":
        if args.tag is not None and not sanitize_name(args.tag):
            parser.error("--tag debe contener un nombre valido; no puede estar vacio.")
        return build_and_backup(args.target, args.tag or "")

    if args.command == "flash":
        return flash_target(args.target, args.port)

    if args.command == "prune":
        try:
            prune_old_backups(args.keep)
        except ValueError as exc:
            parser.error(str(exc))
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
