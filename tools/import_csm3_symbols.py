#!/usr/bin/env python3
"""Import address-bearing function labels from jiangzhengwenjz/csm3.

The importer deliberately consumes symbol evidence only. It does not copy or
compile the decompilation's C, assembly, data, graphics, or scripts.
"""

from __future__ import annotations

import argparse
import pathlib
import re

FUNC_RE = re.compile(r"\b(?P<mode>thumb|arm)_func_start\s+(?P<name>[A-Za-z_]\w*)")
ADDRESS_RE = re.compile(r"(?:^|_)(?P<address>0[238][0-9A-Fa-f]{6})$")


def collect(root: pathlib.Path) -> list[tuple[int, str, str]]:
    found: dict[int, tuple[str, str]] = {}
    for source in sorted((root / "asm").rglob("*.s")):
        for match in FUNC_RE.finditer(source.read_text(encoding="utf-8")):
            name = match.group("name")
            address_match = ADDRESS_RE.search(name)
            if not address_match:
                continue
            address = int(address_match.group("address"), 16)
            found.setdefault(address, (match.group("mode"), name))
    return [(address, *found[address]) for address in sorted(found)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("csm3", type=pathlib.Path, help="local csm3 checkout")
    parser.add_argument("--output", type=pathlib.Path,
                        default=pathlib.Path("symbols/imported_symbols.tsv"))
    args = parser.parse_args()

    symbols = collect(args.csm3)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="\n") as output:
        output.write("# Tab-separated: addr\tmode\tname\n")
        output.write("# Imported from csm3 address-bearing assembly labels.\n")
        output.write("0x08000000\tarm\trom_header_start_vector\n")
        output.write("0x080000C0\tarm\tcrt0_start\n")
        for address, mode, name in symbols:
            output.write(f"0x{address:08X}\t{mode}\t{name}\n")

    print(f"Wrote {len(symbols) + 2} reviewed-format seeds to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
