# Project location

Keep every durable file related to this project inside:

`C:\Users\Nickt\Documents\Codex\Projects\SummonNightSwordcraftStory3Recomp`

This includes source code, submodules, build trees, generated output,
configuration, documentation, logs, screenshots, and validation artifacts.
Temporary editing files outside this directory must be removed when the task
finishes.

# Repository boundaries

- Put game-specific work in this repository.
- Put reusable, game-agnostic GBA runtime work in the `gbarecomp` submodule on
  its dedicated feature branch.
- Put reusable launcher UI work in the `recomp-ui` submodule on its dedicated
  feature branch.
- Never commit ROM, BIOS, save, or ROM-derived generated files.
