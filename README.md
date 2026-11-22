# Relógio Esquisito — Ben 10: Protector of Earth (PS2) Static Recompilation

Recompilação estática de **Ben 10: Protector of Earth** (PS2, NTSC-U, `SLUS-216.61`, VER 1.01) para C nativo em PC (alvo inicial: Linux).

> **Este repositório não contém nenhum arquivo do jogo.** Nenhum binário, ISO, asset, projeto Ghidra ou código gerado derivado do jogo é versionado aqui. Você precisa da sua própria cópia legal da ISO.

## Estado atual

**Ambiente inicial configurado.**
- ISO extraída localmente em `game/` (fora do git).
- ELF principal (`SLUS_216.61`) analisado e importado no projeto Ghidra local (`ghidra/Ben10POE.gpr`, fora do git) com `ghidra-emotionengine-reloaded` (`r5900:LE:32:default`).
- Perfil técnico inicial do binário documentado em [`docs/binario.md`](docs/binario.md).

## Estrutura do repositório

- `docs/`: documentação técnica e notas de engenharia reversa ([perfil do binário](docs/binario.md))
- `progression/`: diário de progresso por data
- `src/`: código-fonte do recompilador / código C portado
- `runtime/`: camada de runtime (substituição de hardware EE/GS/VU/IOP/SPU2 e stubs de SDK)
- `tools/`: scripts auxiliares (extração da ISO, análise headless no Ghidra)

Pastas locais (ignoradas pelo `.gitignore`):
- `game/`: conteúdo extraído da ISO original (`SLUS_216.61`, `SYSTEM.CNF`, `MODULES/`, `GAME.DIR`, `GAME.WAD`, `IOPRP300.IMG`)
- `ghidra/`: projeto Ghidra local (`Ben10POE.gpr` / `Ben10POE.rep`)
- `generated/`: saída C gerada pelo recompilador
- `build/`: artefatos de compilação

## Setup local

1. Extraia a sua ISO original para `game/`:
   ```bash
   tools/extract_iso.sh "/caminho/para/Ben 10 - Protector of Earth (USA).iso"
   ```
2. Importe e analise o ELF no Ghidra (requer Ghidra 12.1.2 + `ghidra-emotionengine-reloaded`):
   ```bash
   tools/ghidra_analyze.sh
   ```
3. Abra `ghidra/Ben10POE.gpr` na GUI do Ghidra para inspeção, renomeação e análise de funções.
