"""Inspect saved guest memory and source descriptors; never decode image data."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def memory(path):
    with Path(path).open('rb') as f:
        header = f.read(52)
        if len(header) != 52 or header[:4] != b'GBAS':
            raise ValueError('Expected a GBAS snapshot')
        version = struct.unpack_from('<I', header, 4)[0]
        if version not in (1, 2):
            raise ValueError('Unsupported snapshot version')
        for _ in range(struct.unpack_from('<I', header, 48)[0]):
            tag, size = struct.unpack('<4sI', f.read(8))
            if tag == b'BUS0':
                if size < 0x48000:
                    raise ValueError('Truncated BUS0 memory')
                data = f.read(0x48000)
                if len(data) != 0x48000:
                    raise ValueError('Truncated BUS0 payload')
                return data[:0x40000], data[0x40000:]
            f.seek(size, 1)
    raise ValueError('No BUS0 section')


def describe(path):
    e, i = memory(path)
    u16 = lambda p, a: struct.unpack_from('<H', p, a)[0]
    u32 = lambda p, a: struct.unpack_from('<I', p, a)[0]
    layers = []
    for bg in range(1, 4):
        d = 0x2a20 + bg*0x34
        w, h, ptr = u16(i,d+4), u16(i,d+6), u32(i,d+0x1c)
        size = (w//8)*(h//8)*2
        valid = w and h and w%8 == h%8 == 0 and 0x02000000 <= ptr <= 0x02040000-size
        layers.append(dict(bg=bg, width=w, height=h, source=hex(ptr),
            sha1=hashlib.sha1(e[ptr-0x02000000:ptr-0x02000000+size]).hexdigest() if valid else None,
            animations=u16(i,d+0x10), scroll=[u16(i,d+8),u16(i,d+10)]))
    return dict(state=str(Path(path).resolve()), sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
        battle=dict(root=hex(u32(i,0x6ac0)), phase=u32(i,0x6ab4), arena=i[0x1a94],
                    enabled=u32(i,0x1a90), mode=i[0xc]), layers=layers)


def battle_assets(path, arena):
    """sub_08001D3C/1D78 archives and 08031420's LZ77 map inputs."""
    rom = Path(path).read_bytes()
    def member(base, index):
        return base + 16*struct.unpack_from('<I', rom, base+8+index*8)[0]
    def lz77(offset):
        if rom[offset] != 0x10:
            raise ValueError('Not a GBA LZ77 map')
        size = int.from_bytes(rom[offset+1:offset+4], 'little')
        if size > 0x40000:
            raise ValueError('Unexpected map size')
        p, result = offset+4, bytearray()
        while len(result) < size:
            flags = rom[p]; p += 1
            for bit in range(7, -1, -1):
                if len(result) == size:
                    break
                if flags & (1 << bit):
                    a, b = rom[p:p+2]; p += 2
                    length, distance = (a >> 4)+3, ((a & 15) << 8)+b+1
                    if distance > len(result) or len(result)+length > size:
                        raise ValueError('Invalid LZ77 reference')
                    for _ in range(length):
                        result.append(result[-distance])
                else:
                    result.append(rom[p]); p += 1
        return result
    d = rom[0xb801cc+arena*28:0xb801cc+(arena+1)*28]
    archive = member(0xbda40c, 6)
    maps = []
    for role, index in [('near', d[1]), ('far', d[17])]:
        if index == 255:
            continue
        ptr = member(archive, index)
        data = lz77(ptr)
        maps.append(dict(role=role, asset=index, address=hex(ptr+0x08000000),
                         header=list(struct.unpack_from('<6HI', data, 16)), size=len(data)))
    return dict(arena=arena, descriptor=d.hex(), maps=maps)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('states', nargs='+', type=Path)
    parser.add_argument('--rom', type=Path)
    args = parser.parse_args()
    print(json.dumps([describe(path) for path in args.states], indent=2))
    if args.rom:
        print(json.dumps([battle_assets(args.rom, arena) for arena in (0,2,3)], indent=2))
