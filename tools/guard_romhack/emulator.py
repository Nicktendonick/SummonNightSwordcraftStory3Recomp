"""Headless mGBA adapter; ordinary-input acceptance tests keep saves in memory.

The optional lab write method is not used by test_emulator.py.
"""
import ctypes as c
import json
import os
from pathlib import Path
from build import ROOT

_dllpath=os.add_dll_directory('C:/msys64/mingw64/bin')
lib=c.CDLL(str(ROOT/'validation/guard-romhack/tooling/mgba-probe.dll'))
for name,args,result in [
    ('open',[c.c_char_p,c.c_char_p],c.c_void_p),('close',[c.c_void_p],None),
    ('step',[c.c_void_p,c.c_uint,c.c_uint],None),
    ('read',[c.c_void_p,c.c_uint,c.c_void_p,c.c_uint],None),
    ('write',[c.c_void_p,c.c_uint,c.c_void_p,c.c_uint],None),
    ('state_size',[c.c_void_p],c.c_uint),
    ('save_state',[c.c_void_p,c.c_void_p],c.c_int),
    ('load_state',[c.c_void_p,c.c_void_p],c.c_int),
    ('save_import',[c.c_void_p,c.c_void_p,c.c_uint],c.c_int)]:
    f=getattr(lib,'probe_'+name); f.argtypes=args; f.restype=result

class Emulator:
    def __init__(self,rom,save=None):
        bios=ROOT.parents[1]/'gbarecomp/bios/gba_bios.bin'
        self.handle=lib.probe_open(os.fsencode(rom),os.fsencode(bios))
        assert self.handle,'mGBA initialization failed'
        if save is not None: assert lib.probe_save_import(self.handle,save,len(save))
    def close(self):
        if self.handle: lib.probe_close(self.handle); self.handle=None
    def step(self,keys=0,frames=1): lib.probe_step(self.handle,keys,frames)
    def read(self,addr,size):
        out=c.create_string_buffer(size);lib.probe_read(self.handle,addr,out,size);return out.raw
    def write(self,addr,data): lib.probe_write(self.handle,addr,data,len(data))
    def save(self):
        out=c.create_string_buffer(lib.probe_state_size(self.handle))
        assert lib.probe_save_state(self.handle,out);return out.raw
    def load(self,state): assert lib.probe_load_state(self.handle,state)
    def state(self):
        ram=self.read(0x03000000,0x8000)
        u32=lambda o:int.from_bytes(ram[o:o+4],'little')
        u16=lambda o:int.from_bytes(ram[o:o+2],'little')
        return dict(root=hex(u32(0x6ac0)),phase=u32(0x6ab4),mode=ram[12],sub=ram[13],pause=ram[15],
                    auto=ram[18],arena=ram[0x1a94],slot=ram[0xa26],guard=bool(u32(0x8c8)&0x100),
                    flags=hex(u32(0x8c8)),timer=u16(0x958),player=ram[0x8c0:0xc50].hex())

if __name__=='__main__':
    save=(ROOT.parents[1]/'release/Portable Camera Edge Test/Saves/english-1.0.6f.eep').read_bytes()
    rom=ROOT/'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba'
    e=Emulator(rom,save)
    try:
        for keys,frames in [(0,1200),(8,3),(0,180),(1,3),(0,240),(1,3),(0,240),(1,3),(0,240)]:
            e.step(keys,frames);s=e.state();s.pop('player');print(keys,frames,s,flush=True)
        (ROOT/'validation/guard-romhack/boot-probe.state').write_bytes(e.save())
    finally:e.close()
