SUMMON NIGHT: SWORDCRAFT STORY 3
SELECT-TO-GUARD -- EXPERIMENTAL GBA PATCH v0.1

WHAT IT DOES
Hold SELECT to use normal Guard in manually controlled battles, without
cycling R to the Guard slot. Release SELECT to stop guarding.
Your selected R ability stays selected. Normal R and B controls still work.
Native restrictions still apply (for example, you cannot freely guard in
midair). This is not an invincibility cheat.

Select replaces the normal auto-battle activation shortcut in combat.
Outside combat it retains its normal behavior. This ROM hack is always on;
it does not add a menu toggle. Keep an unmodified ROM to play without it.

PICK ONE PATCH
1. Select-to-Guard-English-1.0.6f-v0.1.bps
   For a ROM ALREADY translated with Hajimari_no_Ishi_v1.0.6.f.bps.
   Order: original Japanese ROM -> English 1.0.6.f -> this English patch.
   Expected source SHA-1: 6753a22a096b8adaa3a869333b99fcfe29ba1fec
   Expected source CRC32: C76631A9

2. Select-to-Guard-Japanese-v0.1.bps
   For the unmodified Japanese game.
   Expected source SHA-1: 3f5253fcf57e07ce52472bd29a61d16b98a12376
   Expected source CRC32: 12AFAE5D

Both source ROMs are 33,554,432 bytes (32 MiB).
Do not apply both patches. Do not add the translation after the Japanese
Guard patch. Other translation versions and other ROM hacks are unsupported.

HOW TO TRY IT
Back up your ROM and battery save first. Use a BPS-compatible patcher to
apply the correct patch, and save the result as a NEW ROM file. Never bypass
a source-checksum error. Open the new ROM normally in your GBA emulator.
If reusing a battery save, use a COPY named as your emulator expects.
Avoid loading old emulator save states made with a different ROM revision.

This is for ordinary GBA emulators/compatible hardware, NOT the PC port's
ROM importer. The PC port already has its separate Select-to-Guard option.

TEST STATUS
Both language patches passed independent mGBA 0.10.5 gameplay tests in one
early battle, including hold/release, keeping the R selection, pause/resume,
airborne restrictions and a field Select check. Guard flag/timer timing
matched the original game's normal Guard. Another 38,280 machine-code
cases and patch-integrity checks passed.

This is an experimental test release, NOT a full-game certification.
Physical GBA hardware/flashcarts and link play have NOT been tested.
Every boss, story sequence and long play session has NOT been tested.

CONTENTS
Two small BPS patches, these instructions, SHA256SUMS.txt, and Source.zip
with readable patch source, tests and technical notes.
No ROM, BIOS, translation patch, save data or tool binaries are included.

The original game and translation belong to their respective creators.
No PC-port executable or portable build was changed for this ROM hack.
