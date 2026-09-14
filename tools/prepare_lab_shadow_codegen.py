"""Generate private shadow-hook shards; never modify the owner's cached corpus."""
import argparse
from pathlib import Path
import subprocess


HOOKS = {'recompiled_007.cpp': ('080091E0', '00000040'),
         'recompiled_024.cpp': ('080091CE', '000000EF')}


def verify_corpus(out, baseline):
    # Cached objects can only be mixed with these regenerated shards if every
    # other byte of generated C++ agrees. Reject compiler/configuration drift.
    names = {p.name for p in out.iterdir() if p.suffix in ('.cpp', '.h')}
    prior = {p.name for p in baseline.iterdir() if p.suffix in ('.cpp', '.h')}
    if names != prior:
        raise ValueError('Cached guest source set changed; a full rebuild is required')
    for name in sorted(names):
        text = (out / name).read_text()
        if name in HOOKS:
            pc, imm = HOOKS[name]
            decl = f'    uint32_t _imm_{pc} = runtime_thumb_alu_immediate(0x{pc}u, 0x{imm}u);\n'
            if text.count(decl) != 1:
                raise ValueError(f'Missing or ambiguous shadow hook: {name}')
            text = text.replace(decl, '').replace(f'_imm_{pc}', f'0x{imm}u')
        if text != (baseline / name).read_text():
            raise ValueError(f'Unexpected codegen change in {name}; do not reuse cached guest objects')
    print('Verified: only the two configured shadow immediates differ from the cached corpus')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--recompiler', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--baseline', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = a.output.resolve()
    if not out.is_relative_to(root / 'build-native'):
        raise ValueError('Private codegen must stay in this experiment build')
    out.mkdir(parents=True, exist_ok=True)
    config = (root / 'symbols/swordcraft3_jp.toml').read_text()
    config = config.replace('3f5253fcf57e07ce52472bd29a61d16b98a12376',
                            'bb2eebf98deb59bb6218442c2308bb5033ae2915')
    cfg = out / 'beta.toml'
    cfg.write_text(config)
    subprocess.run([str(a.recompiler.resolve()), '--rom', str(a.rom.resolve()),
                    '--config', str(cfg), '--symbols', str(root / 'symbols/imported_symbols.tsv'),
                    '--out', str(out)], check=True)
    verify_corpus(out, a.baseline.resolve())


if __name__ == '__main__':
    main()
