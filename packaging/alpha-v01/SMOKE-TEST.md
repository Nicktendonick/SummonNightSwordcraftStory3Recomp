# Exact-candidate smoke test

Status: **interactive / clean-machine checks NOT RUN** by the packager.

Test the exact archive hash, not the existing build folder. Prefer a separate
Windows x64 VM/machine/profile without MSYS2, compiler/Python tools, old settings
or code caches. An isolated directory on the developer's host is not a clean
machine. Record results without sharing private input files or paths.

- [ ] Record ZIP SHA-256, Windows version, graphics/audio devices and tester.
- [ ] Extract into a fresh writable directory containing spaces.
- [ ] Start `Launch.cmd` with no ROM, BIOS or settings. Check assets and text.
- [ ] Verify missing/wrong ROM, BIOS and translation patch error messages.
- [ ] Select tester-supplied correct inputs, boot and listen to intro audio.
- [ ] Play an overworld route and a fight without existing code caches/tools.
- [ ] Test keyboard/controller input and configuration persistence.
- [ ] Check Native, 16:9, 2:1, full-arena and live Adaptive resizing; verify
      margins, sprites, screen boundaries, effects and battle transitions.
- [ ] Listen and observe pacing during a longer fight; return from FF/rewind.
- [ ] Check all ten slots (slot 10 via menu), rewind, FF speed slider and binds.
- [ ] Make a NEW normal in-game save and reload it after restart. Back up any
      existing saves before testing a different build.
- [ ] Record settings/save/cache locations and any dependency on source-tree
      files, network downloads or developer-only tools.
- [ ] Recheck untouched ZIP/checksums. Do not upload the used test directory.

Automated local checks cover the PE import closure, unchanged loaded binary
sections after debug stripping, explicit archive contents, hashes, an extracted
`--help` launch and invalid-argument rejection with a restricted PATH. Missing
input opens the Windows file picker even with `--no-window`; that behavior
requires an interactive check rather than an automated rejection assertion. They do
not verify audio, graphical rendering, controller behavior or an entire game.
