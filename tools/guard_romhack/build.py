"""Build standalone ARM7TDMI Guard patches; never overwrite an input ROM.

Dependencies: keystone-engine 0.9.2, capstone 5.0.6 (isolated local tooling).
The five long Thumb jumps do not depend on emulator hooks or PC-port code.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'validation/guard-romhack/tooling/python'))
from keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

BASE = 0x08000000
CAVE = 0x09fc0000
SOURCES = {'Japanese': '3f5253fcf57e07ce52472bd29a61d16b98a12376',
           'English-1.0.6f': '6753a22a096b8adaa3a869333b99fcfe29ba1fec'}
RANGES = [(0x270ac,0x3b8,0x77abbd67703dbc58), (0x29930,0xac,0xb09adeb964f2d589),
          (0x412d8,0x4fc,0x4812a2243b4a4ab4), (0x42688,0x23c,0x883c85b0211a847e),
          (0x49018,0x4c,0x720210e6fcce5b3e)]
SITES = [(0x080272e4,12,0x09fc0000,'input'), (0x08029964,12,0x09fc0200,'auto'),
         (0x08042692,10,0x09fc0400,'slot1'), (0x080426d6,10,0x09fc0600,'slot2'),
         (0x0804285a,10,0x09fc0800,'preserve')]

def require(ok, message):
    if not ok: raise ValueError(message)

def fnv(data):
    h=14695981039346656037
    for b in data: h=((h^b)*1099511628211)&0xffffffffffffffff
    return h

def assemble(source, address):
    encoded,_=Ks(KS_ARCH_ARM,KS_MODE_THUMB).asm(source,addr=address)
    require(encoded is not None, 'Assembler returned no code')
    return bytes(encoded)

# No new persistent RAM. All gates derive from the game's existing state.
GATE = '''
    ldr r0, root_ptr
    ldr r0, [r0]
    ldr r1, iwram
    cmp r0, r1
    bne fallback
    ldr r0, phase_ptr
    ldr r0, [r0]
    cmp r0, #4
    bne fallback
    ldrb r0, [r1, #12]
    cmp r0, #2
    bne fallback
    ldrb r0, [r1, #13]
    cmp r0, #3
    beq fallback
    ldrb r0, [r1, #15]
    cmp r0, #0
    bne fallback
    ldrb r0, [r1, #18]
    cmp r0, #0
    bne fallback
'''
HELD = '''
    ldr r0, player
    cmp r4, r0
    bne fallback
    ldr r0, held_ptr
    ldrh r0, [r0]
    movs r1, #4
    tst r0, r1
    beq fallback
'''
POOL = '''
    .p2align 2
root_ptr: .word 0x03006ac0
phase_ptr: .word 0x03006ab4
iwram: .word 0x03000000
player: .word 0x030008c0
held_ptr: .word 0x0300594c
new_ptr: .word 0x03005920
'''

def ret(address, label):
    # Neutral to flags, LR, and ALL general registers. Saved r1's stack slot
    # becomes the return PC; the live r1 register is never modified.
    return f'''push {{r0,r1}}
    ldr r0, {label}
    str r0, [sp,#4]
    pop {{r0,pc}}
    .p2align 2
{label}: .word {address|1}
'''

def stub(name):
    head='pop {r3}\npush {r0-r3}\n'
    if name=='input':
        # 272E4 is also reached by AI/event branches. Only the path that
        # loaded physical input at 272E2 still has held_ptr in r0.
        code=head+'ldr r1, held_ptr\ncmp r0,r1\nbne fallback\n'+GATE+'''
        movs r0,#4
        tst r6,r0
        beq fallback
        bics r6,r0
        movs r0,#2
        orrs r6,r0
fallback:
        pop {r0-r3}
        ldr r4, root_ptr
        ldr r1,[r4]
        ldrb r0,[r1,#13]
        cmp r0,#3
        beq link_return
        movs r2,#0x8c
        '''+ret(0x080272f0,'normal_ret')+'link_return:\n'+ret(0x080272fa,'link_ret')
    elif name=='auto':
        code='pop {r3}\nldr r0,new_ptr\nldrh r1,[r0]\npush {r0-r3}\n'+GATE+'''
        pop {r0-r3}
        movs r0,#4
        bics r1,r0
        b decide
fallback:
        pop {r0-r3}
decide:
        movs r0,#4
        ands r0,r1
        cmp r0,#0
        beq no_toggle
        '''+ret(0x08029970,'yes_ret')+'no_toggle:\n'+ret(0x080299a6,'no_ret')
    elif name in ('slot1','slot2'):
        dest='r5' if name=='slot1' else 'r2'
        # Result selection occurs AFTER restoring registers; do not pop over it.
        replay='movs r1,#0xb3\nlsls r1,r1,#1\nadds r0,r4,r1\n'
        code=head+GATE+HELD+'pop {r0-r3}\n'+replay+f'movs {dest},#0\nb done\nfallback:\npop {{r0-r3}}\n'+replay+f'ldrb {dest},[r0]\ndone:\n'
        code+=('adds r0,r4,#0\n'+ret(0x0804269c,'return_pc') if name=='slot1'
               else 'cmp r2,#0\n'+ret(0x080426e0,'return_pc'))
    else:
        replay='movs r0,#0xb3\nlsls r0,r0,#1\nadds r5,r4,r0\nmovs r0,#0\n'
        code=head+GATE+HELD+'pop {r0-r3}\n'+replay+ret(0x08042868,'guard_ret')
        code+='fallback:\npop {r0-r3}\n'+replay+'strb r0,[r5]\n'+ret(0x08042864,'normal_ret')
    return code+POOL

def trampoline(address, target):
    return assemble(f'''push {{r3}}
    ldr r3, dest
    bx r3
    .p2align 2
dest: .word {target|1}
''', address)

def identity(data):
    return dict(size=len(data),sha1=hashlib.sha1(data).hexdigest(),
                sha256=hashlib.sha256(data).hexdigest(),crc32=f'{zlib.crc32(data):08x}')

def build(source):
    sha1=hashlib.sha1(source).hexdigest()
    label=next((k for k,v in SOURCES.items() if v==sha1),None)
    require(label is not None,'Unsupported ROM. Use exact Japanese or English 1.0.6.f base.')
    require(len(source)==0x2000000,'Unexpected ROM length')
    for a,n,h in RANGES: require(fnv(source[a:a+n])==h,'Reviewed routine changed')
    require(not any(source[CAVE-BASE:]),'Expected trailing zero padding was changed')
    result=bytearray(source); manifest=[]
    for site,n,target,name in SITES:
        asm=stub(name); payload=assemble(asm,target); jump=trampoline(site,target)
        require(len(jump)==n and len(payload)<0x200,'Bad assembly size')
        result[site-BASE:site-BASE+n]=jump
        result[target-BASE:target-BASE+len(payload)]=payload
        manifest.append(dict(name=name,site=f'{site:08x}',size=n,target=f'{target:08x}',
                             payload_size=len(payload),assembly=asm))
    require(result[:0xc0]==source[:0xc0],'Cartridge header changed')
    return label,bytes(result),manifest

def varint(n):
    out=bytearray()
    while True:
        byte=n&127; n>>=7
        if not n: return bytes(out+bytes([byte|128]))
        out.append(byte); n-=1

def bps(source,target,label):
    # Only SourceRead and TargetRead; never embeds unchanged game assets.
    meta=('Select-to-Guard experimental v0.1; base='+label).encode()
    out=bytearray(b'BPS1'+varint(len(source))+varint(len(target))+varint(len(meta))+meta)
    i=0
    while i<len(target):
        same=source[i]==target[i]; end=i+1
        while end<len(target) and (source[end]==target[end])==same: end+=1
        out+=varint(((end-i-1)<<2)|(0 if same else 1))
        if not same: out+=target[i:end]
        i=end
    out+=struct.pack('<II',zlib.crc32(source),zlib.crc32(target))
    out+=struct.pack('<I',zlib.crc32(out))
    return bytes(out)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom',type=Path); parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args(); source=args.rom.read_bytes(); label,target,manifest=build(source)
    args.out.mkdir(parents=True,exist_ok=False)
    (args.out/'patched.gba').write_bytes(target) # PRIVATE; not a distribution artifact.
    patch=bps(source,target,label)
    (args.out/f'Select-to-Guard-{label}-v0.1.bps').write_bytes(patch)
    (args.out/'build.json').write_text(json.dumps(dict(base=label,source=identity(source),
        target=identity(target),patch=identity(patch),hooks=manifest),indent=2)+'\n')
    require(args.rom.read_bytes()==source,'Source changed during build')
    print(label, 'built', len(patch),'byte BPS; source preserved')

if __name__=='__main__': main()
