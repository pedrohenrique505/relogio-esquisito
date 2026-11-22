#!/usr/bin/env bash
# Importa e analisa SLUS_216.61 no Ghidra headless (projeto em ghidra/Ben10POE).
# Uso: tools/ghidra_analyze.sh [caminho/do/SLUS_216.61]
# Requer: Ghidra em /opt/ghidra (ou GHIDRA_HOME) e extensão ghidra-emotionengine-reloaded.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${1:-$ROOT/game/SLUS_216.61}"
GHIDRA="${GHIDRA_HOME:-/opt/ghidra}"
PROJ_DIR="$ROOT/ghidra"
PROJ_NAME="Ben10POE"
NAME="$(basename "$ELF")"

if [ ! -f "$ELF" ]; then
  echo "ELF não encontrado: $ELF" >&2
  echo "Extraia a ISO do jogo em $ROOT/game/ primeiro." >&2
  exit 1
fi

mkdir -p "$PROJ_DIR"

if [ -d "$PROJ_DIR/$PROJ_NAME.rep" ]; then
  echo "Projeto Ghidra já existe em $PROJ_DIR/$PROJ_NAME.gpr"
  echo "Para reimportar do zero, remova $PROJ_DIR/$PROJ_NAME.{gpr,rep} antes."
  exit 0
fi

"$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ_NAME" \
  -import "$ELF" \
  -processor "r5900:LE:32:default"
