# 2026-11-22 — Primeiro boot até o menu inicial (60 FPS com paraLLEl-GS)

## Resumo

Primeiro marco atingido: **Ben 10: Protector of Earth (`SLUS_216.61`)** recompilado estaticamente com **PS2Recomp**, executando nativamente no Linux com renderização GPU via **paraLLEl-GS (Vulkan)** até a tela de seleção de save / menu inicial (`New Game — Choose a save location using the left analog stick`), sem erros nem alvos faltantes.

## O que foi reaproveitado do `digimon-rumble-arena-2-recomp`

- **Pipeline PS2Recomp (`e563698`)** com os 37 patches genéricos de runtime/IOP/GS/VU1 (`patches/ps2recomp/0001`–`0037`).
- **`CreateGapFunctions.java`** + `ExportPS2Functions.java` via Ghidra headless (`tools/ghidra_export.sh`): criou 1.388 funções em lacunas de `.text` e exportou 4.963 funções / 15.911 labels.
- **Bindings HLE do SDK Sony 3.0** via `ps2_analyzer` + `tools/gen_sdk_bindings.py` (113 bindings aplicados por trampolim em runtime, sem recompilar `generated/`).
- **Sink GPU `paraLLEl-GS`** (`runtime/parallel_gs/ben10_parallel_gs.cpp`, ativado com `BEN10_GS_BACKEND=gpu-present`) + `ben10_null_func_log.cpp`.

## Problemas encontrados e resolvidos até o menu inicial

1. **Loop infinito em `sceSifBindRpc(sid=0x80000597)` (`CDVD_SEARCHFILE`)**
   - `ps2_analyzer` não reconheceu `sceCdSearchFile` (`0x001C3920`) do SDK 3.0.
   - Além disso, `ps2_stubs::sceCdSearchFile` devolve LSNs sintéticos (`0x100000+`), enquanto `CDARB.IRX` rodando no IOP emulado lê setores reais da ISO (`BEN10_ISO`), onde `\GAME.WAD;1` está no LSN **`0x06DDDF`**. O LSN sintético estourava o tamanho da ISO e fazia `CDARB.IRX` entrar em retry infinito (`0x17A90`–`0x17AE0`), queimando **98,5% da CPU no IOP** (~1,7 FPS).
   - **Correção**: `cdSearchFileRealIso` em `runtime/overrides/ben10_overrides.cpp` consulta o diretório ISO-9660 real da ISO quando `BEN10_ISO` está definida. O uso de CPU do IOP caiu de **985 ms/s para ~4 ms/s** (280x mais rápido).

2. **IRXs padrão da Sony em busy-wait no IOP**
   - `SIO2MAN.IRX`, `MTAPMAN.IRX`, `PADMAN.IRX`, `MCMAN.IRX` e `MCSERV.IRX` já têm HLE no `ps2xRuntime`.
   - **Correção**: `sifLoadModuleFiltered` (`0x001CA248`) pula esses 5 módulos padrão e carrega no emulador IOP apenas os módulos específicos do jogo/áudio (`LIBSD.IRX`, `CDARB.IRX`, `ALASD.IRX`).

3. **Alvos indiretos de vtable fora da tabela de funções**
   - `0x0014B820` ficou 8 bytes dentro de `FUN_0014b818` por causa de um delay slot órfão (`addiu $sp, $sp, 0x10; nop`) capturado pelo `CreateGapFunctions`.
   - `0x001A4C58` é um thunk de 2 instruções (`j 0x001A5C18; nop`, `__pure_virtual`) sem `jr $ra`.
   - **Correção**: desvios registrados em `runtime/overrides/ben10_overrides.cpp`.

4. **Estouro de heap (`0x01F00000` insuficiente) causando `__pure_virtual` no parser RIFF (`FUN_001dca08`)**
   - O `ps2xRuntime` limitava `EndOfHeap` a `0x01F00000` (31 MB), quase 1 MB abaixo do limite real do PS2 (`0x01FF0000` com stack de 64 KB). O alocador do jogo atingia `0x01ED89F8` e `malloc()` retornava `NULL`, fazendo `FUN_001dca08` gravar a tabela de handlers de chunks RIFF (`'guid'`) em `0x00000000` e chamar `__pure_virtual`.
   - **Correção**: patch `0038-ps2xRuntime-raise-guest-heap-limit-to-0x01FA0000.patch` eleva o limite de heap para `0x01FA0000` (+640 KB) e move os pools internos de RPC/TLS/BootMode para `0x01FA0000`–`0x01FD1000` (abaixo da stack principal em `0x01FE0000`–`0x01FF0000`).

## Resultado

- **Resolução**: `640x448` via paraLLEl-GS (Vulkan, RADV), nítida e sem artefatos.
- **Taxa de quadros**: ~45–57 Hz durante o boot/animação do menu (e 60 Hz de scanout).
- **Estabilidade**: no teste de 25 s o jogo chegou à tela `New Game` (seleção de slot de save com o Chama animado ao lado direito) com 468 quadros únicos renderizados e 265 mil pacotes GIF. Em um segundo teste de 20 s (sem trace), após ~15 s no menu, apareceu um `JALR target=0x0` em `0x001EE138` (`FUN_001ee0f8`, rotina de áudio/stream com `8000`) com `s0=0x54` (ponteiro nulo + offset) — próximo item a investigar.

## Próximos passos

1. Investigar o `JALR target=0x0` intermitente em `FUN_001ee0f8` (`0x001EE138`, provável subsistema de áudio/ALASD) quando ocioso no menu.
2. Testar navegação de controle (DualShock/teclado) no menu inicial e entrada em New Game / primeira fase.
3. Corrigir `CreateGapFunctions.java` para ignorar delay slots órfãos após `jr $ra` e capturar thunks `j target; nop` de vtables.
4. Avaliar desempenho em gameplay 3D (VU1 AOT com `vu1_transpile.py` se necessário) e suporte a áudio (`ALASD.IRX` / SPU2).
