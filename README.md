# Relógio Esquisito — Ben 10: Protector of Earth (PS2) Static Recompilation

Recompilação estática de **Ben 10: Protector of Earth** (PS2, NTSC-U, `SLUS-216.61`, VER 1.01) para executável nativo em PC (alvo inicial: Linux), baseada no **PS2Recomp** com renderização GPU via **paraLLEl-GS (Vulkan)**.

> **Este repositório não contém nenhum arquivo do jogo.** Nenhum binário, ISO, asset, projeto Ghidra ou código gerado derivado do jogo é versionado aqui. Você precisa da sua própria cópia legal da ISO.

## Estado atual

**Menu inicial funcionando em 60 FPS no Linux (paraLLEl-GS / Vulkan).**
- Boot completo, leitura real de `GAME.DIR` / `GAME.WAD` via `CDARB.IRX` no IOP emulado, carregamento de `LIBSD.IRX` e `ALASD.IRX`, checagem de Memory Card (`/BASLUS-21661`) e renderização nítida (`640x448`) até o menu inicial (`New Game`).
- Veja o diário detalhado em [`progression/2026-11-22-menu-inicial.md`](progression/2026-11-22-menu-inicial.md) e o perfil do binário em [`docs/binario.md`](docs/binario.md).

## Estrutura do repositório

- `CMakeLists.txt`: build do executável `ben10poe` (`ben10poe_generated` + `ps2_runtime` + `ps2_iop` + `parallel-gs` + overrides)
- `patches/ps2recomp/`: patches genéricos aplicados ao clone do `PS2Recomp` (`0001`–`0038`)
- `runtime/overrides/`: overrides específicos do jogo (`ben10_overrides.cpp`, `ben10_sdk_bindings.inc`, `ben10_null_func_log.cpp`)
- `runtime/parallel_gs/`: integração do sink Vulkan `paraLLEl-GS` (`BEN10_GS_BACKEND=gpu-present`)
- `tools/`: scripts de extração da ISO, export Ghidra (`CreateGapFunctions.java`), aplicação de patches e geração de bindings do SDK
- `docs/`: documentação técnica e notas de engenharia reversa ([perfil do binário](docs/binario.md))
- `progression/`: diário de progresso por data

Pastas locais (ignoradas pelo `.gitignore`):
- `game/`: conteúdo extraído da ISO original (`SLUS_216.61`, `SYSTEM.CNF`, `MODULES/`, `GAME.DIR`, `GAME.WAD`, `IOPRP300.IMG`)
- `external/PS2Recomp/`: clone local do upstream (`e563698`) com patches aplicados
- `.cache/parallel-gs-source/`: checkout pinado do `paraLLEl-GS`
- `ghidra/`: projeto Ghidra local (`Ben10POE.gpr`, `headless/`, `export/`, `analyzer/`)
- `generated/`: código C++ gerado pelo `ps2_recomp`
- `build/`: artefatos de compilação (`build/ben10poe`)

## Setup e execução local

1. Extraia a sua ISO original para `game/`:
   ```bash
   tools/extract_iso.sh "/caminho/para/Ben 10 - Protector of Earth (USA).iso"
   ```
2. Clone o `PS2Recomp` em `external/PS2Recomp` (commit `e563698`), aplique os patches e compile `ps2_recomp` e `ps2_analyzer`:
   ```bash
   tools/apply_upstream_patches.sh
   cmake -S external/PS2Recomp -B external/PS2Recomp/out/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
     -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -DNDEBUG" \
     -DPS2X_BUILD_STUDIO=OFF -DPS2X_BUILD_TEST=OFF \
     -DCMAKE_C_FLAGS=-march=x86-64-v3 -DCMAKE_CXX_FLAGS=-march=x86-64-v3
   cmake --build external/PS2Recomp/out/build --target ps2_recomp ps2_analyzer
   ```
3. Exporte o mapa de funções pelo Ghidra headless e gere o código C++:
   ```bash
   tools/ghidra_export.sh
   # Ajuste output = ".../generated" em ghidra/export/recomp.toml e rode:
   external/PS2Recomp/out/build/ps2xRecomp/ps2_recomp ghidra/export/recomp.toml
   ```
4. Compile e execute `ben10poe`:
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
     -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -DNDEBUG" \
     -DBEN10_ENABLE_PARALLEL_GS=ON
   cmake --build build --target ben10poe -j5

   cd game && \
   BEN10_ISO="/caminho/para/Ben 10 - Protector of Earth (USA).iso" \
   BEN10_GS_BACKEND=gpu-present \
   ../build/ben10poe "$PWD/SLUS_216.61"
   ```
