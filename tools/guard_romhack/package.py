"""Package only original source, documentation and validated BPS patches."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
from build import ROOT

def main():
    private=ROOT/'validation/guard-romhack'
    for name in ('cpu','patch','emulator'):
        assert json.loads((private/(name+'-report.json')).read_text())['passed']
    destination=ROOT.parents[1]/'release/Select-to-Guard GBA Patch v0.1'
    archive=destination.parent/(destination.name+'.zip')
    assert not destination.exists() and not archive.exists(),'Refusing to overwrite a package'
    patches=[]
    for folder in ('english-v01','japanese-v01'):
        report=json.loads((private/folder/'build.json').read_text())
        patch=next((private/folder).glob('*.bps'))
        assert hashlib.sha256(patch.read_bytes()).hexdigest()==report['patch']['sha256']
        patches.append(patch)
    destination.mkdir(parents=True)
    for patch in patches:shutil.copyfile(patch,destination/patch.name)
    source=Path(__file__).parent
    shutil.copyfile(source/'PLAYER_README.txt',destination/'README.txt')
    names=('build.py','test_cpu.py','test_patch.py','test_emulator.py','emulator.py',
           'mgba_probe.c','package.py','README.md','PLAYER_README.txt')
    with zipfile.ZipFile(destination/'Source.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name in names:z.write(source/name,'tools/guard_romhack/'+name)
    files=sorted(destination.iterdir())
    checks=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in files)
    (destination/'SHA256SUMS.txt').write_text(checks)
    expected={p.name for p in patches}|{'README.txt','Source.zip','SHA256SUMS.txt'}
    assert {p.name for p in destination.iterdir()}==expected
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(destination.iterdir()):z.write(p,destination.name+'/'+p.name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert len(z.namelist())==len(expected)
    print(archive)
    print('SHA256',hashlib.sha256(archive.read_bytes()).hexdigest())

if __name__=='__main__':main()
