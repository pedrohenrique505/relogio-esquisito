#!/usr/bin/env bash
# Executa Ben 10: Protector of Earth (SLUS_216.61) com paraLLEl-GS (Vulkan).
# Uso: tools/run.sh [caminho/para/Ben 10 - Protector of Earth (USA).iso]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_ISO="$HOME/Downloads/Ben 10 - Protector of Earth (USA)/Ben 10 - Protector of Earth (USA).iso"
ISO="${1:-${BEN10_ISO:-$DEFAULT_ISO}}"

if [ ! -f "$ISO" ]; then
  echo "ISO não encontrada: $ISO" >&2
  echo "Passe o caminho como argumento: tools/run.sh \"/caminho/para/Ben 10.iso\"" >&2
  exit 1
fi

if [ ! -x "$ROOT/build/ben10poe" ]; then
  echo "Executável não encontrado: $ROOT/build/ben10poe" >&2
  exit 1
fi

cd "$ROOT/game"
exec env \
  BEN10_ISO="$ISO" \
  BEN10_GS_BACKEND="${BEN10_GS_BACKEND:-gpu-present}" \
  "$ROOT/build/ben10poe" "$ROOT/game/SLUS_216.61"
