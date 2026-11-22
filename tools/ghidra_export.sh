#!/usr/bin/env bash
# Importa e analisa SLUS_216.61 no Ghidra headless (projeto separado do da GUI)
# e roda CreateGapFunctions.java + ExportPS2Functions.java do PS2Recomp,
# gerando TOML + CSV em ghidra/export/.
#
# Uso: tools/ghidra_export.sh [caminho/do/SLUS_216.61]
# Requer: Ghidra em /opt/ghidra (ou GHIDRA_HOME), plugin ghidra-emotionengine-reloaded,
#         e external/PS2Recomp (contém ExportPS2Functions.java).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ELF="${1:-$ROOT/game/SLUS_216.61}"
GHIDRA="${GHIDRA_HOME:-/opt/ghidra}"
SCRIPT_DIR="${EXPORT_SCRIPT_DIR:-$ROOT/external/PS2Recomp/ps2xRecomp/tools/ghidra}"
PROJ_DIR="$ROOT/ghidra/headless"
OUT_DIR="$ROOT/ghidra/export"
NAME="$(basename "$ELF")"

mkdir -p "$PROJ_DIR" "$OUT_DIR"

SCRIPT=(-scriptPath "$ROOT/tools/ghidra;$SCRIPT_DIR"
        -postScript CreateGapFunctions.java
        -postScript ExportPS2Functions.java
        "$OUT_DIR/$NAME.toml" "$OUT_DIR/$NAME.csv")

if [ -d "$PROJ_DIR/Ben10POE_headless.rep" ]; then
  "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" Ben10POE_headless \
    -process "$NAME" -noanalysis "${SCRIPT[@]}"
else
  "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" Ben10POE_headless \
    -import "$ELF" -processor "r5900:LE:32:default" "${SCRIPT[@]}"
fi

echo "Saída em: $OUT_DIR"
ls -la "$OUT_DIR"
