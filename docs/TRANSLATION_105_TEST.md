# English translation 1.0.5.f: isolated portable test

Historical experiment record. The follow-up portable recipe now selects this
released-English/Japanese pair and keeps released-English saves separate from
old beta saves. See [bounded-view and release migration notes](BOUNDED_BATTLE_VIEW.md).
The statements below about retaining the old normal package describe the
earlier isolated test, not the subsequent migration.

This experiment does not update `release/Portable Beta`, publish a ZIP, or
change the normal English-beta target. No ROM, patch, BIOS, save, generated
guest source, or private validation payload belongs in Git.

## Identities and generation

The supplied BPS has source CRC32 `12AFAE5D`, target `CC25DCDC`, and patch
CRC32 `F0D5B3C9`. The existing patch tool validated all three checksums while
writing a new private ROM copy. The released 32 MiB ROM has:

- SHA-1: `06a9f4db52f40a7034dc1c74161a705f30edb858`
- SHA-256: `38c26be256623cf2806388840cfcf1597f634ad8932943009cb65020efbe89b1`

Set `SWORDCRAFT3_RELEASE105_ROM` to that private file to enable CMake targets
`Swordcraft3Translation105` and `Swordcraft3Japanese105`. The English engine
regenerates every guest shard; it does not reuse old English-beta objects.
The Japanese engine retains the original Japanese corpus, with the new
revision's English identity for sibling-engine routing. The two binaries are
renamed to the standard engine filenames **inside the separate test package**.

The base Japanese generation configuration is combined with
`symbols/swordcraft3_release105.toml`. Standard targets retain their old ROM
identities. A freshly recompiled header test verifies the release identity;
private-input tests also check that each English variant rejects the other.

## Reviewed compatibility

`tools/validate_translation105.py` authenticates 39 unchanged code ranges:
Guard, battle ownership, spell-window producers, culling instructions,
RAM-code copies, base indirect-call tables, and field producers/consumers.
These are bounded compatibility checks, not proof of every gameplay scene.

The broad field range `08093994..080A6A4C` changes only three bytes in the
four-byte literal at `080A4560`. Original/beta pointer `080C0178` becomes
`08004194`, the translated expiry message. The literal load at `080A4538`
passes it to `080A53AC`; all surrounding field instructions remain identical.
That action is not added to the renderer's accepted tool callbacks. The new
whole-ROM field identity is enabled only with the release-test compile flag;
live ownership, source-map, animation and control-state guards remain intact.

## Discovered script dispatch gap

An initial 3,946-frame cold-boot input probe found one interpreter fallback at
`08012692` (18 interpreted instructions, automatic healing disabled). This
address is **not** the same instruction as at that address in the Japanese
source: the translation replaces the script evaluator's implementation.

The patched instructions at `08012632..0801263E` subtract opcode `0x82`, bound
the index to `0x0F`, scale it by four, calculate table `080126C0` with ADR,
load its word and execute `MOV PC,r0`. All 16 entries are even Thumb targets.
The release-only configuration names the complete table rather than adding
one observed PC or editing generated output. SHA-256 of the reviewed
`1262C..12700` ROM-offset span is
`ab552abace428953c76f2a66d8a28930b576babc76db184023cffa77c569dc25`.

## Packaging and verification

`tools/package_translation105_test.py` stages a clean file allowlist, audits
PE imports and stripping, copies private inputs only for this local test, and
keeps save folders empty. Tests run in another disposable copy, not in the
delivered payload. Whole-install hashes protect the main Portable Beta;
source-input hashes protect the original ROM, BIOS and patch. Installation
creates a unique sibling folder and is gated on passing smoke evidence.

The smoke covers real launcher patch import/on/off, Japanese without a patch,
both directions of sibling-engine handoff, cold boot, Guard authentication,
live graphics settings and reset. It uses state and log evidence, not pixels.
The longer input probe uses a fresh game and never loads an old beta state.
Per-run private JSON/log records under `validation/` are the authority for
the exact tested executable and results.

The corrected 3,946-step cold-boot/input replay passed with
`FULLY_STATIC`, zero dispatch misses and zero interpreted instructions
(`validation/translation105-1790799151217705900`). The route stayed outside
field/battle ownership; this is startup/script coverage, not combat evidence.
All 21 selected non-image CTests passed, as did private-input language tests
for release-English/JP acceptance and old-English-beta rejection, and the
inverse check on the unchanged normal beta identity.

Not yet certified: general old battery/save-state compatibility, a full
translated story playthrough, every arena-specific encounter and special
effect, or visual approval.
The subsequent [arena audit](ARENA_AUDIT_20260930.md) tests all 17 numbered arena
backgrounds plus four duplicate slots using a new-release encounter fixture.
It is separate evidence from startup success and does not certify every story
battle or effect. Four arenas widen; the others retain native-width fallback.
