#!/usr/bin/env bash
set -euo pipefail

rg -n "Context\.h|Config\.h|Arduino\.h|WiFi\.h|SD\.h|Preferences\.h|WebServer\.h" include/Eolo/Core include/Eolo/Types && {
  echo "Forbidden dependency found under include/Eolo/Core or include/Eolo/Types" >&2
  exit 1
}

echo "Eolo Core/Types dependency audit OK"
