# Required cartridge image

Supply your own legally dumped Japanese cartridge image at
`roms/swordcraft3_jp.gba`.

| Field | Expected value |
|---|---|
| Size | `33,554,432` bytes |
| Game code | `B3CJ` |
| Maker code | `D9` |
| SHA-1 | `3f5253fcf57e07ce52472bd29a61d16b98a12376` |
| SHA-256 | `39bc4cf448106aa4b8cdde235632ffb57432c4b1919c8843510b70b3787fad2d` |
| CRC32 | `12afae5d` |

The ROM is never committed or packaged. The runtime and launcher refuse an
image that does not match the pinned SHA-1.

## Required GBA BIOS

Place a legally obtained standard Game Boy Advance BIOS at
`gbarecomp/bios/gba_bios.bin`.

| Field | Expected value |
|---|---|
| Size | `16,384` bytes |
| SHA-1 | `300c20df6731a33952ded8c436f7f186d25d3492` |
| SHA-256 | `fd2547724b505f487e4f7b229bef8f6b1ed7a41937d4a8413f9478bfdf57070d` |

The supplied local file matches this standard BIOS identity. It is ignored and
is never committed or packaged; its generated recompilation output is private
build material as well.
