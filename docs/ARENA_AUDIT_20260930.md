# Released translation arena audit

The English 1.0.5.f test engine loaded all 17 numbered arena descriptors and
completed a short input smoke test in each. Four additional descriptor slots
are byte-identical and also loaded. This is a background/arena matrix with one
encounter setup, not certification of every boss, story event or special effect.

## Results

| Descriptor IDs | Count | Loading and input smoke | Presentation |
| --- | ---: | --- | --- |
| 0, 2, 3, 7 | 4 | Passed | Complete widescreen composition engaged |
| 1, 4, 5, 6, 8 through 16 | 13 | Passed | Intentional native-width fallback |
| 17 through 20 | 4 identical records | Passed | Native-width fallback; purpose not established |

The matrix covers 12,845 stepped frames. Each case reached battle lifecycle 4,
mode 2 with the requested arena ID. Inputs included left/right, jump, four
attack pulses, Select-Guard press/hold/release and native Start pause/resume.
All cases recorded 30 held-Guard frames, zero Guard frames after release,
15 paused observations, resumed play and changing jump coordinates. All test
processes exited cleanly. Input delivery does not certify every attack's damage
or every collision boundary.

A separate native auto-battle pass reached the result lifecycle and returned
to a verified field owner in all 21 cases, with clean process exits. That pass
stepped another 23,352 frames, for 36,197 frames across the two matrices.
Result dismissal used ordinary A inputs; no HP, enemy or AI memory was edited.

The four supported arenas each recorded 605 active wide frames. The remaining
arenas were not silently counted as widescreen successes. Production eligibility
was not expanded or weakened to make the tests pass.

## How inaccessible arenas were tested

A copied battery save was loaded by the actual 1.0.5.f engine. Normal inputs
reached a field and then combat; no old English-beta save-state header was
rewritten to pretend compatibility. A new-release snapshot was captured at
battle lifecycle 2, immediately before the normal arena initializer.

The private harness changes only script variable `0x1A1` in each disposable
copy. `08012F60` resolves that byte through the pointer at `03006584`.
`080264CC` reads it before selecting assets, and `08031420` performs normal
arena loading. The original arena is 3; that matrix case changes no bytes.
All other state bytes are preserved. This is deliberately a synthetic arena
selection, not proof that each arena was reached through its own story route.

The descriptor table spans `08B801CC..08B80418` in 28-byte records. IDs 0..16
identify their own records; 17..20 are identical. The complete table, variable
reader, battle initializer, lifecycle dispatcher and background loader were
byte-compared with the Japanese ROM and match. Both layers' compressed map
metadata were decoded where present. No images were used for scene recognition,
correctness assertions or framebuffer comparisons.

Five synthetic harness tests cover exact-byte selection, wrong ROM rejection,
wrong lifecycle/root rejection, invalid RAM pointers and malformed containers.

## Limits and remaining work

- This is not a full story playthrough, visual approval or audio/performance
  benchmark. Native-width fallback means those arenas still lack custom wide
  support; it does not mean the battle failed to load.
- One encounter was reused across the matrix. Arena-specific bosses, script
  variants, rare spells, transitions into adjacent story scenes and complete
  movement-boundary coverage remain untested.
- Automatic code healing was disabled. All cases used interpreted RAM routines
  at `0300057C`, `03000694` and `03003240`; a few cases also reached additional
  ROM fallbacks. These runs are **not fully static**. Similar dynamic-RAM
  interpretation is documented for the older beta in `VALIDATION.md`; the
  instruction counts alone do not establish a new performance regression.
- Loading one copied battery save successfully does not certify every old save.
  The delivered translation-test folder still starts with empty save folders.
- No arena-audit hooks were added to the game, no main Portable Beta files were
  replaced, and nothing was committed, pushed or published.

## Reproduction and private evidence

Use `tools/audit_release105_arenas.py`; the disposable fixture and its inputs
must remain private and must never be committed. It verifies the new ROM SHA-1
`06a9f4db52f40a7034dc1c74161a705f30edb858` and rejects a loaded battle when
preparing a changed arena selection. The fixture is not a player save.

- `validation/arena-audit-1790800735901668300`: authenticated source spans,
  descriptor inventory and pre-load fixture provenance.
- `validation/arena-audit-1790800955778446100`: all 21 entry/input runs, executable
  and fixture identities, per-frame state records and renderer decisions.
- `validation/arena-audit-1790801363515196900`: separate native auto-battle and
  result/field-return probes; all 21 passed. Per-arena input states remained
  unchanged. Five synthetic safety tests and `git diff --check` also passed.

These are private local evidence paths, not release assets. The supplied ROM,
patch, BIOS, original battery and source snapshots were kept intact.
Whole-folder hashes also confirmed both the main Portable Beta and the delivered
`release/Translation 1.0.5f Test 20260930-161733` still match their pre-audit
records. Source and diagnostics remain uncommitted.
