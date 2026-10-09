# Translation 1.0.5.f — private test build

Open **Swordcraft Story 3 Beta.exe** in this folder. The window title identifies
this as the Translation 1.0.5.f Test. Close other copies of the game first.

This is a separate portable test, not an update to your main Portable Beta.
Its settings, saves, save states, logs, ROMs and BIOS stay in this folder.
The English 1.0.5.f ROM is selected initially. Start a **new game** for testing.
No existing saves or save states were imported. Compatibility with old beta
save states or battery saves has not been established; keep originals safe.

## Japanese and English

- Home → Change ROM: choose **ROMs/Swordcraft Story 3 Japanese.gba** for Japanese.
- For English, select **ROMs/Swordcraft Story 3 English 1.0.5f.gba** directly.
- Alternatively select the Japanese ROM, then enable
  **Mods/Hajimari_no_Ishi_v1.0.5.f.bps** in the Mods page. Disable that patch to
  return to Japanese. Japanese does not require a translation patch.
- Each language has separate battery saves and save states.
- The previous English beta ROM is intentionally unsupported in this test.

Guard remains optional in Mods. Existing widescreen choices, graphics presets,
filters and the Esc menu are retained. Runtime reset deliberately starts a
fresh engine process.

## Test scope

The build uses a freshly generated instruction corpus for the released patch.
Guest-code checks protect Guard, battle-window hooks, culling instructions and
RAM-resident code copies. Launcher patch import, language handoff, startup and
reset are checked separately. These checks are **not** a full-game playthrough;
later translated scenes, battles and save compatibility still need playtesting.

## Private local copy — do not upload

This local convenience copy contains your private ROMs, BIOS and patch. Do not
upload or distribute this folder. A future public package must be built from a
clean allowlist without those inputs, saves, settings or diagnostic captures.
Nothing from this test has been published to GitHub.
