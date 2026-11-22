#!/usr/bin/env bash
# Extrai os arquivos da ISO de Ben 10: Protector of Earth (PS2) para game/.
# Uso: tools/extract_iso.sh [caminho/da/iso]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFAULT_ISO="$HOME/Downloads/Ben 10 - Protector of Earth (USA)/Ben 10 - Protector of Earth (USA).iso"
ISO="${1:-$DEFAULT_ISO}"

if [ ! -f "$ISO" ]; then
  echo "ISO não encontrada: $ISO" >&2
  exit 1
fi

mkdir -p "$ROOT/game"
bsdtar -xf "$ISO" -C "$ROOT/game"
chmod -R u+rwX "$ROOT/game"
echo "Extraído em $ROOT/game:"
ls -la "$ROOT/game"
