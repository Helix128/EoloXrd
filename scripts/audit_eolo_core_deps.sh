#!/usr/bin/env bash
set -euo pipefail

core_root="lib/EoloCore/include/Eolo"
forbidden='Context\.h|Config/|Config\.h|Arduino\.h|FreeRTOS|RTClib\.h|WiFi\.h|SD\.h|Preferences\.h|WebServer\.h'

rg -n "$forbidden" "$core_root/Core" "$core_root/Types" && {
  echo "Forbidden dependency found under EoloCore" >&2
  exit 1
}

echo "EoloCore dependency audit OK"
