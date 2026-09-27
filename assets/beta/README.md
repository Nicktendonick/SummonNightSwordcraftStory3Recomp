# Beta launcher artwork

Owner-supplied on 2026-09-24: `Swordcraft 3 icon edited by novel ai.png`.
The original is preserved without modification as `beta-icon-source.png`.
Source SHA-256: `886ff8ada0cda210f38e847486e48bc99637c511357594748d9e123fcc6a5bb7`.

`tools/prepare_beta_icon.ps1` converts it into a multi-resolution Windows ICO
(16, 24, 32, 48, 64, 128, 256). No new artwork was generated. The RC resource is
embedded in the local beta entry point and custom-renderer game executable.

This records the supplied asset's origin, not a determination of redistribution
rights. Review artwork provenance with the other release assets before publishing.

## Alternating launcher covers

Two additional owner-supplied PNGs, preserved unchanged on 2026-09-24:

- `boxart-clean.png`: larger text-free artwork; SHA-256
  `28c37aadaa07a8ca3a7478926129355e2079dd36d4311e96e09d231e60ceb825`.
- `boxart-original.png`: original Japanese cover; SHA-256
  `0a7cdbcfd4f8f15d5741787d9b141ae14c7a5de7c66d90fe941807f3cbfc6bcd`.

The native beta starter alternates covers on successive launches, beginning with
the clean artwork. recomp-ui fits the entire image to the existing box-art panel
without stretching or cropping. The Windows icon is independent and unchanged.
