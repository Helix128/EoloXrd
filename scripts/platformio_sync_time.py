import os
import subprocess
import sys

Import("env")


def project_option(name):
    try:
        value = env.GetProjectOption(name)
    except Exception:
        return None
    return value if value else None


def sync_time(*args, **kwargs):
    project_dir = env.subst("$PROJECT_DIR")
    script = os.path.join(project_dir, "scripts", "send_host_time.py")
    command = [sys.executable, script]

    port = project_option("monitor_port") or project_option("upload_port") or env.subst("$UPLOAD_PORT")
    if port and "$" not in port:
        command.append(port)

    baud = project_option("monitor_speed")
    if baud:
        command.extend(["--baud", str(baud)])

    return subprocess.call(command)


env.AddCustomTarget(
    name="sync_time",
    dependencies=None,
    actions=[sync_time],
    title="Sync RTC from host",
    description="Send the current host time to the EOLO serial terminal.",
)
