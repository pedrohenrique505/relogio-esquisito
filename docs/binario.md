# Perfil do binário — `SLUS_216.61`

Análise inicial da ISO **Ben 10: Protector of Earth (USA)** (PS2, NTSC-U).

## Identificação

- **Boot (`SYSTEM.CNF`)**: `BOOT2=cdrom0:\SLUS_216.61;1`, `VER=1.01`, `VMODE=NTSC`
- **Tamanho do ELF**: `1.464.396` bytes (~1,40 MB)
- **CRC32**: `0xD7C918F3`
- **SHA-256**: `f9f1efe049d17729ce9c4a57dcdfd4a82d28c481f4193be207c9da84e3008f03`
- **Formato**: ELF 32-bit LSB, MIPS R5900 (`5900, eabi64, mips3`), statically linked, **stripped** (sem `.symtab`)
- **Entry point**: `0x00100008`
- **Global pointer (`$gp`)**: `0x0025D3F0` (de `.reginfo`)
- **Processador Ghidra**: `r5900:LE:32:default` (`ghidra-emotionengine-reloaded`)

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

## Conteúdo do disco

- `SLUS_216.61` — executável principal do EE
- `SYSTEM.CNF` — configuração de boot
- `IOPRP300.IMG` — imagem IOP do Sony SDK 3.0
- `GAME.DIR` (30 KB) e `GAME.WAD` (~2,45 GB) — índice e arquivo único de dados do jogo
- `MODULES/` — 15 módulos IRX:
  - Padrão Sony: `SIO2MAN.IRX`, `MTAPMAN.IRX`, `PADMAN.IRX`, `MCMAN.IRX`, `MCSERV.IRX`, `LIBSD.IRX`, `USBD.IRX`, `USBKB.IRX`
  - Áudio / streaming / middleware: `CDARB.IRX`, `ALASD.IRX`, `SN_CDARB.IRX`, `SN_ALASD.IRX`, `CDVDSTM.IRX`, `SCREAM.IRX`, `STREAM.IRX`

## Bibliotecas e indícios encontrados nas strings

- **Sony PS2 SDK 3.0**: `libgraph` (`sceGs*`), `libpad`, `libmc`, `sceCd*`, `sceSif*`
- **Bink Video**: `CBinkPlayer` (vídeos anamórficos 16:9 / 4:3)
- **Scripting interno**: funções `ass_*` (inclusive referências compartilhadas com a versão de Wii, ex.: `ass_WiiIsBatteryCritical`)
