# 2026-11-22 — Setup inicial do ambiente

## O que foi feito

- Criado o repositório local em `/home/zathura/Projects/relogio-esquisito` conectado ao remote `git@github.com:pedrohenrique505/relogio-esquisito.git`.
- Configurado `.gitignore` bloqueando qualquer arquivo de copyright do jogo (ISO, ELF, IRX, WAD/DIR, `game/`), projetos Ghidra (`ghidra/`), código gerado (`generated/`) e builds (`build/`).
- Extraída a ISO `Ben 10 - Protector of Earth (USA).iso` para `game/`.
- Realizada inspeção inicial do executável `SLUS_216.61` (`docs/binario.md`):
  - ELF32 MIPS R5900 stripped, ~1,4 MB, `.text` com ~1,06 MB (`0x00100000`–`0x0020fae0`), entry `0x00100008`, `$gp = 0x0025D3F0`.
  - `.vutext` (~57 KB) + 31 overlays `.DVP.overlay.*` (VU1).
  - IOP SDK 3.0 (`IOPRP300.IMG`) + 15 módulos IRX em `MODULES/`.
- Criado projeto Ghidra em `ghidra/Ben10POE.gpr` com `SLUS_216.61` importado e auto-analisado com `r5900:LE:32:default`.

## Próximos passos

1. Definir a arquitetura do recompilador estático para C (pipeline de lifting MIPS R5900 → C, representação de registradores/memória e runtime).
2. Identificar e nomear no Ghidra as syscalls do kernel EE e funções do Sony SDK 3.0 (`sceGs*`, `sceSif*`, `sceCd*`, `libpad`, `libmc`, libc).
3. Exportar o mapa de funções do Ghidra e gerar o primeiro build de teste do binário recompilado em C.
