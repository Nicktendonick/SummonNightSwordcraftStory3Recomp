"""Execute real patched Thumb code against original instructions + Guard model."""
import json
from pathlib import Path
import random
import struct
from build import *
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import *

REGS=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,
      UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,
      UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,
      UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC,UC_ARM_REG_CPSR]
STOPS={'input':{0x080272f0,0x080272fa},'auto':{0x08029970,0x080299a6},
       'slot1':{0x0804269c},'slot2':{0x080426e0},'preserve':{0x08042868}}

class CPU:
    def __init__(self,rom,reference):
        self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB); self.reference=reference
        self.u.mem_map(BASE,0x2000000); self.u.mem_write(BASE,rom)
        self.u.mem_map(0x03000000,0x8000)
        self.instructions={}
        self.decoder=Cs(CS_ARCH_ARM,CS_MODE_THUMB)
        self.u.hook_add(UC_HOOK_CODE,self.hook)
    def hook(self,u,pc,size,_):
        if pc in STOPS[self.name]:
            if self.reference and self.active and self.name=='preserve':
                u.reg_write(UC_ARM_REG_R0,0)
            self.stopped=True; u.emu_stop(); return
        if not self.reference and pc not in self.instructions:
            op=next(self.decoder.disasm(bytes(u.mem_read(pc,4)),pc,count=1))
            # Every injected instruction must be ordinary ARMv4T Thumb-1.
            # Literal words are excluded by checking executed addresses only.
            allowed={'push','pop','ldr','ldrb','ldrh','str','strb','movs','adds',
                     'lsls','cmp','beq','bne','b','bx','bics','orrs','ands','tst'}
            assert op.size==2 and op.mnemonic in allowed,(hex(pc),op.mnemonic,op.op_str)
            self.instructions[pc]=op.mnemonic+' '+op.op_str
        if not self.reference or not self.active: return
        if self.name=='auto' and pc==0x08029968:
            u.reg_write(UC_ARM_REG_R1,u.reg_read(UC_ARM_REG_R1)&~4)
        if self.name=='slot1' and pc==0x0804269a: u.reg_write(UC_ARM_REG_R5,0)
        if self.name=='slot2' and pc==0x080426de: u.reg_write(UC_ARM_REG_R2,0)
        if self.name=='preserve' and pc==0x08042864:
            u.mem_write(0x03000a26,bytes([self.slot]))
    def run(self,name,site,ram,regs,active,slot):
        self.name=name; self.active=active; self.slot=slot; self.stopped=False
        u=self.u; u.mem_write(0x03000000,bytes(ram))
        u.reg_write(UC_ARM_REG_CPSR,regs[-1])
        for reg,value in zip(REGS[:-1],regs[:-1]): u.reg_write(reg,value)
        if self.reference and name=='input' and active:
            value=u.reg_read(UC_ARM_REG_R6)
            u.reg_write(UC_ARM_REG_R6,(value&~4)|(2 if value&4 else 0))
        u.emu_start(site|1,BASE+0x2000000,count=1000)
        assert self.stopped,(name,'execution did not return')
        values=[u.reg_read(r) for r in REGS]
        # Stack scratch below original SP is not persistent game state.
        return values,bytes(u.mem_read(0x03000000,0x7000))

def main():
    rom=(ROOT/'build-native/translation-1.0.6f/Swordcraft Story 3 English 1.0.6f.gba').read_bytes()
    _,patched,_=build(rom); old=CPU(rom,True); new=CPU(patched,False)
    rng=random.Random(731); count=0
    cases=[{}]+[{'phase':n} for n in range(16)]+[{'mode':n} for n in range(12)]
    cases += [{'pause':1},{'pause':255},{'auto':1},{'auto':2},{'sub':3},
              {'root':0x03000100},{'actor':0x03000c50},{'physical':False}]
    for context in cases:
      keys_list=range(1024) if not context else [0,2,4,6,0x104,0x44,0x3ff]
      for keys in keys_list:
       for slot in range(6):
        ram=bytearray(0x8000); phase=context.get('phase',4); mode=context.get('mode',2)
        root=context.get('root',0x03000000); actor=context.get('actor',0x030008c0)
        struct.pack_into('<I',ram,0x6ac0,root); struct.pack_into('<I',ram,0x6ab4,phase)
        ram[12]=mode; ram[13]=context.get('sub',0); ram[15]=context.get('pause',0)
        ram[18]=context.get('auto',0); ram[0xa26]=slot; ram[0xdb6]=slot
        struct.pack_into('<H',ram,0x594c,keys); struct.pack_into('<H',ram,0x5920,keys)
        manual=root==0x03000000 and phase==4 and mode==2 and ram[13]!=3 and not ram[15] and not ram[18]
        held=manual and bool(keys&4) and actor==0x030008c0
        for site,n,target,name in SITES:
            regs=[rng.getrandbits(32) for _ in REGS]
            regs[0]=0x0300594c if context.get('physical',True) else 0
            regs[4]=actor; regs[6]=keys; regs[13]=0x03007e00
            regs[14]=0x08010001; regs[15]=site; regs[16]=(rng.randrange(16)<<28)|0x3f
            active=manual if name=='auto' else (manual and bool(regs[0]) if name=='input' else held)
            expected=old.run(name,site,ram,regs,active,slot)
            actual=new.run(name,site,ram,regs,active,slot)
            assert actual==expected,(context,keys,slot,name,
                [(i,hex(a),hex(b)) for i,(a,b) in enumerate(zip(actual[0],expected[0])) if a!=b])
            count+=1
    report=dict(passed=True,executed_cases=count,all_1024_inputs=True,all_six_slots=True,
        negative_contexts=len(cases)-1,register_flags_stack_and_game_ram_parity=True,
        armv4t_thumb1_executed_opcode_audit=True,instructions=new.instructions,
        limitation='Unicorn instruction test, not GBA timing or physical hardware certification')
    (ROOT/'validation/guard-romhack/cpu-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print({k:v for k,v in report.items() if k!='instructions'})

if __name__=='__main__': main()
