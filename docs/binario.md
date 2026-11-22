# Perfil do binário — `SLUS_216.61`

Análise da ISO **Ben 10: Protector of Earth (USA)** (PS2, NTSC-U).

## Identificação

- **Boot (`SYSTEM.CNF`)**: `BOOT2=cdrom0:\SLUS_216.61;1`, `VER=1.01`, `VMODE=NTSC`
- **Tamanho do ELF**: `1.464.396` bytes (~1,40 MB)
- **CRC32**: `0xD7C918F3`
- **SHA-256**: `f9f1efe049d17729ce9c4a57dcdfd4a82d28c481f4193be207c9da84e3008f03`
- **Formato**: ELF 32-bit LSB, MIPS R5900 (`5900, eabi64, mips3`), statically linked, **stripped** (sem `.symtab`)
- **Entry point**: `0x00100008`
- **Global pointer (`$gp`)**: `0x0025D3F0` (de `.reginfo`)
- **Processador Ghidra**: `r5900:LE:32:default` (`ghidra-emotionengine-reloaded`)
- **Contagem de funções**: 3.575 na auto-análise do Ghidra; **4.963 funções** e **15.911 labels** após `CreateGapFunctions.java` (20.874 funções geradas pelo `ps2_recomp`, 0 erros).

## Seções principais

| Seção | Endereço | Tamanho | Flags | Observações |
| --- | --- | --- | --- | --- |
| `.text` | `0x00100000` | `0x10fae0` (~1,06 MB) | AX | Código MIPS R5900 principal |
| `.vutext` | `0x0020fae0` | `0x00e4f0` (~57 KB) | AX | Microcódigo VU |
| `.data` | `0x0021e000` | `0x01ad98` (~107 KB) | WA | Dados inicializados |
| `.rodata` | `0x00238e00` | `0x01c5a8` (~113 KB) | A | Constantes e strings |
| `.sbss` | `0x00255400` | `0x000034` | WAp | Small BSS (`$gp`) |
| `.bss` | `0x00255480` | `0x030b4c` (~195 KB) | WA | Dados não inicializados |
| `.DVP.overlay.*` (31 seções) | — | variado | WX | Overlays de microcódigo VU1 + `.DVP.ovlytab` |

Não há `.gcc_except_table` (sem exceções C++ habilitadas).

## Mapa de funções-chave (boot e SDK)

| Endereço | Papel | Observações |
| --- | --- | --- |
| `0x00100008` | `entry` | Zera `.sbss`/`.bss` (`0x00255400`–`0x00285FCC`), chama `InitMainThread` (stack 64 KB) / `InitHeap` (`-1`), construtores globais e `main()` |
| `0x00133030` | `main` | Chama init (`0x00143660`) e loop principal (`0x00144B88`) |
| `0x00142DC0` | `initIopAndCd` | `sceGsResetPath`, `sceDmaReset`, `sceVpu0Reset`, `sceSifRebootIop("cdrom0:\IOPRP300.IMG;1")`, `sceSifSyncIop`, reinicia SIF/FS/CD |
| `0x0012B7A0` | `initGameWad` | Lê `cdrom0:\GAME.DIR` (tabela de entradas de `0x48` bytes) e chama `sceCdSearchFile("\GAME.WAD;1")` |
| `0x0012FD30` | `loadBootIrx` | Carrega 8 IRXs via `0x0012FCD8` (`sceSifLoadModule`): `SIO2MAN`, `MTAPMAN`, `PADMAN`, `MCMAN`, `MCSERV`, `LIBSD`, `CDARB`, `ALASD` |
| `0x001C3920` | `sceCdSearchFile` | Não detectada pelo `ps2_analyzer`; ligada via override com consulta ISO-9660 real |
| `0x001C53A8` | `write` (libkernel) | TTY DECI2; substituída por `ttyWrite` para `stdout`/`stderr` do host |
| `0x001C82B0` | `sceSifBindRpc` | Override bloqueante que avança ciclos EE/IOP até o servidor IOP registrar o SID |
| `0x001CA248` | `_sceSifLoadModule` | Override que pula os 5 IRXs padrão já cobertos por HLE e carrega `LIBSD`, `CDARB` e `ALASD` no IOP |
| `0x001A4C58` | `__pure_virtual` | Thunk `j 0x001A5C18; nop` (`terminate` → `0x001A5C08`) |

## Memória e heap

- O alocador do jogo consome acima de `0x01ED89F8` ainda no carregamento inicial (parser de chunks RIFF em `FUN_001dca08` / `FUN_001d7240`).
- No PS2 real, `InitHeap(..., -1)` entrega até `0x01FF0000` (abaixo da stack de 64 KB). No `ps2xRuntime`, o limite padrão `0x01F00000` estoura o heap; o patch `0038` sobe o limite para `0x01FA0000` (+640 KB).

## Conteúdo do disco e LSNs reais (ISO-9660)

- `SLUS_216.61` (LSN `0x192B15`) — executável principal do EE
- `SYSTEM.CNF` (LSN `0x192DE1`) — configuração de boot
- `IOPRP300.IMG` (LSN `0x192A8E`) — imagem IOP do Sony SDK 3.0
- `GAME.DIR` (LSN `0x06DDD0`, 30 KB) e `GAME.WAD` (LSN `0x06DDDF`, ~2,45 GB) — índice e arquivo único de dados do jogo; o `CDARB.IRX` lê diretamente por LSN real da ISO
- `MODULES/` — 15 módulos IRX no disco (8 carregados no boot):
  - Padrão Sony (HLE no runtime): `SIO2MAN.IRX`, `MTAPMAN.IRX`, `PADMAN.IRX`, `MCMAN.IRX`, `MCSERV.IRX`, `USBD.IRX`, `USBKB.IRX`
  - Áudio / CD / middleware (emulados no IOP): `LIBSD.IRX`, `CDARB.IRX`, `ALASD.IRX` (e presentes no disco: `SN_CDARB.IRX`, `SN_ALASD.IRX`, `CDVDSTM.IRX`, `SCREAM.IRX`, `STREAM.IRX`)

## Bibliotecas e middleware

- **Sony PS2 SDK 3.0**: 322 símbolos reconhecidos pelo `ps2_analyzer`, dos quais 113 ativos como bindings HLE (`libgraph`, `libdma`, `libpad`, `libmc`, `libcdvd`, `sifrpc`, `sifcmd`, `libvu0`).
- **Bink Video**: `CBinkPlayer` (vídeos anamórficos 16:9 / 4:3).
- **Scripting interno**: funções `ass_*` (inclusive referências compartilhadas com a versão de Wii, ex.: `ass_WiiIsBatteryCritical`).
