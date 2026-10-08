"""BPS integrity and exact-change audit, independently decoded from the builder."""
import json
import struct
import zlib
from build import ROOT,BASE,CAVE,SITES,assemble,stub,build,identity

def apply_bps(source,patch):
    def check(ok,msg):
        if not ok:raise ValueError(msg)
    check(len(patch)>=16 and patch[:4]==b'BPS1','Bad patch header')
    source_crc,target_crc,patch_crc=struct.unpack('<III',patch[-12:])
    check(zlib.crc32(patch[:-4])==patch_crc,'Patch checksum mismatch')
    check(zlib.crc32(source)==source_crc,'Wrong source ROM')
    pos=4
    def number():
        nonlocal pos
        value=0;shift=1
        for _ in range(10):
            check(pos<len(patch)-12,'Truncated number')
            digit=patch[pos];pos+=1;value+=(digit&127)*shift
            if digit&128:return value
            shift<<=7;value+=shift
        raise ValueError('Oversized number')
    source_size=number();target_size=number();meta_size=number()
    check(source_size==len(source) and target_size==len(source),'Unexpected sizes')
    pos+=meta_size;check(pos<=len(patch)-12,'Metadata out of range')
    target=bytearray()
    while len(target)<target_size:
        command=number();kind=command&3;size=(command>>2)+1
        check(size<=target_size-len(target),'Output overflow')
        if kind==0:target+=source[len(target):len(target)+size]
        elif kind==1:
            check(pos+size<=len(patch)-12,'Truncated literal')
            target+=patch[pos:pos+size];pos+=size
        else:raise ValueError('Unexpected copy mode in this patch')
    check(pos==len(patch)-12,'Trailing commands')
    check(zlib.crc32(target)==target_crc,'Target checksum mismatch')
    return bytes(target)

def rejected(callback):
    try:callback()
    except ValueError:return
    raise AssertionError('Invalid input accepted')

def main():
    out=ROOT/'validation/guard-romhack'; reports=[]
    for folder,rom in [('english-v01',ROOT/'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'),
                       ('japanese-v01',ROOT.parents[1]/'ROMs/swordcraft3_jp.gba')]:
        source=rom.read_bytes();target=(out/folder/'patched.gba').read_bytes()
        patch=next((out/folder).glob('*.bps')).read_bytes()
        assert apply_bps(source,patch)==target
        label,rebuilt,_=build(source);assert rebuilt==target
        spans=[]
        for site,n,cave,name in SITES:
            spans += [(site-BASE,site-BASE+n),(cave-BASE,cave-BASE+len(assemble(stub(name),cave)))]
        end=0
        for start,stop in sorted(spans):
            assert source[end:start]==target[end:start],hex(start)
            end=stop
        assert source[end:]==target[end:] and source[:0xc0]==target[:0xc0]
        rejected(lambda:apply_bps(source[:-1],patch))
        corrupt=bytearray(patch);corrupt[80]^=1
        rejected(lambda:apply_bps(source,corrupt))
        # Valid patch checksum but bad target checksum must also fail.
        corrupt=bytearray(patch);corrupt[-8]^=1
        struct.pack_into('<I',corrupt,len(corrupt)-4,zlib.crc32(corrupt[:-4]))
        rejected(lambda:apply_bps(source,corrupt))
        wrong=bytearray(source);wrong[0x100]^=1
        rejected(lambda:build(wrong));rejected(lambda:build(target))
        rejected(lambda:apply_bps(wrong,patch))
        assert rom.read_bytes()==source
        reports.append(dict(base=label,patch=identity(patch),source=identity(source),target=identity(target),
                            independent_bps_roundtrip=True,deterministic_rebuild=True,
                            changes_only_five_hooks_and_padding=True,invalid_inputs_rejected=True))
    report=dict(passed=True,cases=reports)
    (out/'patch-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: both BPS files reproduce tested ROMs; only allowed bytes changed; invalid inputs rejected.')

if __name__=='__main__':main()
