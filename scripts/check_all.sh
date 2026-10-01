#!/usr/bin/env bash
# Verificacion completa sin placa: compila todos los entornos de firmware,
# corre los tests nativos y Python y los chequeos estaticos del repositorio.
#
# Uso: scripts/check_all.sh [--with-hw-suites] [--with-demos]
#   --with-hw-suites  compila (sin ejecutar) las suites de test/ en eolo_dron
#   --with-demos      compila todos los entornos demo_* de platformio.demos.ini
set -u
cd "$(dirname "$0")/.."

with_suites=0
with_demos=0
for arg in "$@"; do
  case "$arg" in
    --with-hw-suites) with_suites=1 ;;
    --with-demos) with_demos=1 ;;
    *) echo "opcion desconocida: $arg" >&2; exit 2 ;;
  esac
done

failures=()
step() {
  local name=$1
  shift
  echo "==> $name"
  if ! "$@"; then
    failures+=("$name")
  fi
}

for env in $(grep -oE '^\[env:eolo_[a-z0-9_]+' platformio.ini | sed 's/\[env://'); do
  step "build $env" pio run -e "$env"
done

step "tests nativos" pio test -e native
step "tests Python" python3 -m unittest discover -s test -p 'test_*.py'
step "pinouts" python3 scripts/check_pinouts.py
step "limites entre familias" python3 scripts/check_family_boundaries.py
step "dependencias de EoloCore" bash scripts/audit_eolo_core_deps.sh
step "git diff --check" git diff --check

if [ "$with_suites" -eq 1 ]; then
  for suite in $(ls test | grep '^test_' | grep -v '\.py$' | grep -v '^test_eolo_core_native$'); do
    step "suite $suite (compilar)" pio test -e eolo_dron -f "$suite" --without-uploading --without-testing
  done
fi

if [ "$with_demos" -eq 1 ]; then
  for env in $(grep -oE '^\[env:demo_[a-z0-9_]+' platformio.demos.ini | sed 's/\[env://'); do
    step "build $env" pio run -e "$env"
  done
fi

echo
if [ "${#failures[@]}" -eq 0 ]; then
  echo "TODO OK"
  exit 0
fi
echo "FALLARON ${#failures[@]}:"
printf '  - %s\n' "${failures[@]}"
exit 1
