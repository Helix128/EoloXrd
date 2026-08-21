import sys
from pathlib import Path

Import("env")

# Asegurar que el directorio de scripts esté en sys.path
project_dir = Path(env.subst("$PROJECT_DIR"))
scripts_dir = project_dir / "scripts"
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))

try:
    from firmware_backup import create_backup, list_all_backups
except ImportError:
    # Si por alguna razón no se puede importar directamente
    import imp
    fb = imp.load_source("firmware_backup", str(scripts_dir / "firmware_backup.py"))
    create_backup = fb.create_backup
    list_all_backups = fb.list_all_backups


def _post_build_backup(source, target, env):
    """Acción posterior ejecutada al finalizar la generación de firmware.bin."""
    try:
        build_dir = Path(env.subst("$BUILD_DIR"))
        pioenv = env.subst("$PIOENV")

        demo_name = ""
        demo_model = ""
        try:
            demo_name = env.GetProjectOption("custom_demo_name", "")
            demo_model = env.GetProjectOption("custom_demo_model", "")
        except Exception:
            pass

        create_backup(
            build_dir=build_dir,
            env_name=pioenv,
            project_dir=project_dir,
            demo_name=demo_name,
            demo_model=demo_model,
        )
    except Exception as e:
        print(f"[AUTO-BACKUP AVISO] No se pudo completar el auto-respaldo: {e}")


# Conectar hook a la generación del binario final
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _post_build_backup)

# Agregar Custom Target de PlatformIO para listar respaldos desde el IDE
env.AddCustomTarget(
    "backups",
    None,
    lambda *args, **kwargs: list_all_backups(),
    title="List Firmware Backups",
    description="Mostrar lista de respaldos de firmware guardados.",
)
