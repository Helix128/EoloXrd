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
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
BACKUPS_DIR = PROJECT_ROOT / "firmware_backups"
DEVICES_BACKUP_DIR = BACKUPS_DIR / "devices"
DEMOS_BACKUP_DIR = BACKUPS_DIR / "demos"
RELEASES_BACKUP_DIR = BACKUPS_DIR / "releases"
OTHER_BACKUP_DIR = BACKUPS_DIR / "other"
BUILD_TAG_FILE = PROJECT_ROOT / ".build_tag"

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
    return re.sub(r"_+", "_", clean).strip("_")


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
    """Calcula el numero de compilacion secuencial del dia para este target."""
    if not target_dir.exists():
        return 1
    count = 0
    for folder in target_dir.iterdir():
        if folder.is_dir() and folder.name.startswith(date_str):
            count += 1
    return count + 1


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

    snapshot_dir_name = f"{timestamp_prefix}_{version_tag}"
    snapshot_dir = target_folder / snapshot_dir_name
    snapshot_dir.mkdir(parents=True, exist_ok=True)

    base_name = f"{friendly_label}_{timestamp_prefix}_{version_tag}"
    fw_dst_name = f"{base_name}_firmware.bin"
    bl_dst_name = f"{base_name}_bootloader.bin"
    pt_dst_name = f"{base_name}_partitions.bin"
    elf_dst_name = f"{base_name}_firmware.elf"
    map_dst_name = f"{base_name}_firmware.map"

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

    update_latest_folder(target_folder, snapshot_dir, friendly_label, metadata, fw_dst_name, bl_dst_name, pt_dst_name)

    fw_size_kb = files_info["firmware"]["size_bytes"] / 1024.0
    rel_path = snapshot_dir.relative_to(project_dir)
    print("\n" + "=" * 60)
    print(f" [RESPALDO GUARDADO] {friendly_label}")
    print(f"  Version/Tag : {version_tag}")
    print(f"  Tamano      : {fw_size_kb:.1f} KB")
    print(f"  Ubicacion   : {rel_path}")
    print(f"  Flasheo     : {rel_path / 'flash_firmware.sh'}")
    print("=" * 60 + "\n")

    return snapshot_dir


def update_latest_folder(target_folder: Path, snapshot_dir: Path, friendly_label: str, metadata: dict,
                         fw_name: str, bl_name: str, pt_name: str) -> None:
    """Mantiene una carpeta latest/ actualizada con la version mas reciente."""
    latest_dir = target_folder / "latest"
    latest_dir.mkdir(parents=True, exist_ok=True)

    latest_fw_name = f"{friendly_label}_latest_firmware.bin"
    latest_bl_name = f"{friendly_label}_latest_bootloader.bin"
    latest_pt_name = f"{friendly_label}_latest_partitions.bin"

    if (snapshot_dir / fw_name).exists():
        shutil.copy2(snapshot_dir / fw_name, latest_dir / latest_fw_name)
    if (snapshot_dir / bl_name).exists():
        shutil.copy2(snapshot_dir / bl_name, latest_dir / latest_bl_name)
    if (snapshot_dir / pt_name).exists():
        shutil.copy2(snapshot_dir / pt_name, latest_dir / latest_pt_name)

    generate_flashing_scripts(latest_dir, latest_fw_name, latest_bl_name, latest_pt_name, f"{friendly_label} (Latest)")

    latest_meta = dict(metadata)
    latest_meta["is_latest"] = True
    latest_meta["source_snapshot"] = snapshot_dir.name
    (latest_dir / "build_info.json").write_text(json.dumps(latest_meta, indent=2, ensure_ascii=False), encoding="utf-8")


def name_last_backup(new_name: str, target_env: str = "") -> bool:
    """Renombra o re-etiqueta el ultimo respaldo generado."""
    clean_name = sanitize_name(new_name)
    if not clean_name:
        print("Error: debes proporcionar un nombre valido.")
        return False

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
    
    folder_parts = last_dir.name.split("_")
    if len(folder_parts) >= 2 and re.match(r"^\d{4}-\d{2}-\d{2}$", folder_parts[0]):
        timestamp_prefix = f"{folder_parts[0]}_{folder_parts[1]}"
    else:
        timestamp_prefix = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

    new_folder_name = f"{timestamp_prefix}_{clean_name}"
    new_dir = parent_dir / new_folder_name

    if new_dir == last_dir:
        print(f"El respaldo ya tiene el nombre: {clean_name}")
        return True

    last_dir.rename(new_dir)

    base_name = f"{friendly_label}_{timestamp_prefix}_{clean_name}"
    new_fw_name = f"{base_name}_firmware.bin"
    new_bl_name = f"{base_name}_bootloader.bin"
    new_pt_name = f"{base_name}_partitions.bin"

    for file_path in new_dir.iterdir():
        if not file_path.is_file():
            continue
        fname = file_path.name
        if fname.endswith("_firmware.bin"):
            file_path.rename(new_dir / new_fw_name)
        elif fname.endswith("_bootloader.bin"):
            file_path.rename(new_dir / new_bl_name)
        elif fname.endswith("_partitions.bin"):
            file_path.rename(new_dir / new_pt_name)
        elif fname.endswith("_firmware.elf"):
            file_path.rename(new_dir / f"{base_name}_firmware.elf")
        elif fname.endswith("_firmware.map"):
            file_path.rename(new_dir / f"{base_name}_firmware.map")

    meta["version_tag"] = clean_name
    meta["renamed_at"] = datetime.datetime.now().isoformat()
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
    for root, dirs, files in os.walk(BACKUPS_DIR):
        p = Path(root)
        if p.name == "latest" or not (p / "build_info.json").exists():
            continue
        try:
            meta = json.loads((p / "build_info.json").read_text(encoding="utf-8"))
            fw_size = meta.get("files", {}).get("firmware", {}).get("size_bytes", 0)
            backups.append({
                "path": p.relative_to(PROJECT_ROOT),
                "target": meta.get("target", "unknown"),
                "label": meta.get("label", "unknown"),
                "category": meta.get("category", "unknown"),
                "tag": meta.get("version_tag", ""),
                "date": meta.get("created_at", meta.get("timestamp", ""))[:19],
                "size_kb": f"{fw_size / 1024.0:.1f} KB" if fw_size else "-",
            })
        except Exception:
            continue

    if not backups:
        print("\nNo se encontraron respaldos validos en firmware_backups/.\n")
        return

    backups.sort(key=lambda x: x["date"], reverse=True)

    print("\n" + "=" * 90)
    print(f"{'FECHA':<20} | {'OBJETIVO / DEMO':<24} | {'TAG / VERSION':<22} | {'TAMANO':<10}")
    print("-" * 90)
    for b in backups:
        print(f"{b['date']:<20} | {b['label']:<24} | {b['tag']:<22} | {b['size_kb']:<10}")
    print("=" * 90)
    
    current_tag = get_session_tag()
    if current_tag:
        print(f"(*) Nombre de version actual activo: '{current_tag}'")
    print("")


def prune_old_backups(keep_count: int = 10) -> None:
    """Conserva solo las ultimas N versiones por target."""
    if keep_count < 1:
        keep_count = 1

    targets = {}
    for root, dirs, files in os.walk(BACKUPS_DIR):
        p = Path(root)
        if p.name == "latest" or not (p / "build_info.json").exists():
            continue
        parent = p.parent
        targets.setdefault(parent, []).append(p)

    deleted = 0
    for parent, snapshots in targets.items():
        if len(snapshots) <= keep_count:
            continue
        snapshots.sort(key=lambda x: x.stat().st_mtime, reverse=True)
        to_delete = snapshots[keep_count:]
        for snap in to_delete:
            shutil.rmtree(snap, ignore_errors=True)
            deleted += 1

    print(f"Limpieza completada: {deleted} respaldos antiguos eliminados (se conservan {keep_count} por target).")


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
        env_vars["EOLO_TAG"] = tag

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
    backup_p.add_argument("--tag", default="", help="Tag opcional para el respaldo.")

    build_p = sub.add_parser("build", help="Compilar un dispositivo y respaldar automaticamente.")
    build_p.add_argument("target", help="Target a compilar (ej. express, dron, standard, eolo_express).")
    build_p.add_argument("--tag", default="", help="Tag opcional para esta compilacion.")

    flash_p = sub.add_parser("flash", help="Flashear el ultimo respaldo de un target.")
    flash_p.add_argument("target", help="Target a flashear (ej. express, dron, standard).")
    flash_p.add_argument("--port", default="", help="Puerto serial (ej. /dev/ttyUSB0 o COM3).")

    prune_p = sub.add_parser("prune", help="Limpiar respaldos antiguos conservando los ultimos N.")
    prune_p.add_argument("--keep", type=int, default=10, help="Cantidad de versiones a conservar por target (default: 10).")

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
        build_dir = PROJECT_ROOT / ".pio" / "build" / args.env
        create_backup(build_dir, args.env, PROJECT_ROOT, custom_tag=args.tag)
        return 0

    if args.command == "build":
        return build_and_backup(args.target, args.tag)

    if args.command == "flash":
        return flash_target(args.target, args.port)

    if args.command == "prune":
        prune_old_backups(args.keep)
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
