#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from demo_config import MODELS, config_for, demo_sketches, env_name
from generate_demo_envs import main as generate_demo_envs


def available_demos():
    demos = {}
    for sketch in demo_sketches():
        demo_name = sketch.parent.name
        demos[demo_name.lower()] = demo_name
    return demos


def resolve_demo(name):
    demos = available_demos()
    demo = demos.get(name.lower())
    if not demo:
        known = ", ".join(sorted(demos.values()))
        raise SystemExit(f"Demo desconocida: {name}. Disponibles: {known}")
    return demo


def resolve_model(model):
    if model not in MODELS:
        known = ", ".join(MODELS.keys())
        raise SystemExit(f"Modelo desconocido: {model}. Disponibles: {known}")
    return model


def ensure_supported(demo_name, model):
    models = config_for(demo_name)["models"]
    if model not in models:
        raise SystemExit(
            f"{demo_name} no declara soporte para modelo {model}. "
            f"Modelos disponibles: {', '.join(models)}"
        )


def pio_command():
    cmd = shutil.which("pio") or shutil.which("platformio")
    if not cmd:
        raise SystemExit("No se encontro PlatformIO. Instala o agrega 'pio'/'platformio' al PATH.")
    return cmd


def run_pio(args, env_vars=None):
    generate_demo_envs()
    merged_env = os.environ.copy()
    if env_vars:
        merged_env.update(env_vars)
    return subprocess.call([pio_command(), *args], env=merged_env)


def list_demos(_args):
    generate_demo_envs()
    for sketch in demo_sketches():
        demo_name = sketch.parent.name
        models = ", ".join(config_for(demo_name)["models"])
        print(f"{demo_name}: {models}")
    return 0


def build_or_upload(args, target=None):
    demo_name = resolve_demo(args.demo)
    model = resolve_model(args.model)
    ensure_supported(demo_name, model)
    env = env_name(demo_name, model)
    pio_args = ["run", "-e", env]
    if target:
        pio_args.extend(["-t", target])
    extra_env = {}
    if getattr(args, "tag", None):
        extra_env["EOLO_TAG"] = args.tag
    return run_pio(pio_args, env_vars=extra_env)


def monitor(args):
    demo_name = resolve_demo(args.demo)
    model = resolve_model(args.model)
    ensure_supported(demo_name, model)
    env = env_name(demo_name, model)
    return run_pio(["device", "monitor", "-e", env])


def list_backups_cmd(_args):
    scripts_dir = Path(__file__).resolve().parent
    sys.path.insert(0, str(scripts_dir))
    from firmware_backup import list_all_backups
    list_all_backups()
    return 0


def main():
    parser = argparse.ArgumentParser(description="Build/upload helpers for EOLO demos.")
    sub = parser.add_subparsers(dest="command", required=True)

    list_parser = sub.add_parser("list", help="List detected demos and supported models.")
    list_parser.set_defaults(func=list_demos)

    backups_parser = sub.add_parser("backups", help="List all firmware and demo backups.")
    backups_parser.set_defaults(func=list_backups_cmd)

    for command in ("build", "upload"):
        p = sub.add_parser(command, help=f"{command} a demo environment.")
        p.add_argument("demo", help="Demo name, for example AFM07 or BME280.")
        p.add_argument("model", nargs="?", default="dron", help="Board model. Default: dron.")
        p.add_argument("--tag", default="", help="Optional version tag for this build.")
        p.set_defaults(func={
            "build": lambda a: build_or_upload(a),
            "upload": lambda a: build_or_upload(a, "upload"),
        }[command])

    p_mon = sub.add_parser("monitor", help="Monitor serial output for a demo environment.")
    p_mon.add_argument("demo", help="Demo name, for example AFM07 or BME280.")
    p_mon.add_argument("model", nargs="?", default="dron", help="Board model. Default: dron.")
    p_mon.set_defaults(func=monitor)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
