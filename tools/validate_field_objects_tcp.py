"""Exercise the real TCP server: save loading, held keys and both screenshots."""
import argparse, hashlib, json, os, socket, struct, subprocess, time, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OWNER=ROOT.parents[1]

def png(path,w,h,rgb):
    def chunk(k,d): return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d)&0xffffffff)
    raw=b''.join(b'\0'+rgb[y*w*3:(y+1)*w*3] for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

def run(state,out,width,enabled,steps,keys,feature='objects'):
    out.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy()
    env.update(SWORDCRAFT3_CUSTOM_RENDERER='1',SWORDCRAFT3_CUSTOM_HOST_WIDTH=str(width),
        SWORDCRAFT3_CUSTOM_OBJECTS=str(enabled),GBARECOMP_SELFHEAL_RECOMPILE='0',SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy')
    if feature=='battles':
        env.update(SWORDCRAFT3_CUSTOM_OBJECTS='1',SWORDCRAFT3_CUSTOM_BATTLES=str(enabled))
    if feature=='replay':
        env.update(SWORDCRAFT3_CUSTOM_OBJECTS='1',SWORDCRAFT3_CUSTOM_BATTLES='1',
                   SWORDCRAFT3_CUSTOM_REPLAY_CHECK=str(enabled))
    for k in ('GBARECOMP_INPUT_REPLAY','GBARECOMP_INPUT_RECORD','GBARECOMP_VISIBLE_DEBUGGER','GBARECOMP_DEBUG_CAPTURE_DIR'):
        env.pop(k,None)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1',0));port=probe.getsockname()[1]
    args=[str(ROOT/'build-native/Swordcraft3CustomRendererBeta.exe'),'--tcp',str(port),
        '--bios',str(OWNER/'gbarecomp/bios/gba_bios.bin'),'--rom',str(OWNER/'build-beta/rom-patch-cache/swordcraft3_beta.gba'),
        '--save',str(out/'private.eep'),'--view-width','240',str(ROOT/'native-test.toml')]
    records=[];pictures=[];hashes=[]
    with (out/'stdout.log').open('wb') as stdout,(out/'stderr.log').open('wb') as stderr:
        process=subprocess.Popen(args,cwd=ROOT/'build-native',env=env,stdout=stdout,stderr=stderr)
        connection=None
        try:
            deadline=time.monotonic()+30
            while connection is None:
                if process.poll() is not None: raise RuntimeError('TCP executable exited before connection')
                try: connection=socket.create_connection(('127.0.0.1',port),timeout=1)
                except OSError:
                    if time.monotonic()>deadline: raise
                    time.sleep(.1)
            connection.settimeout(30)
            stream=connection.makefile('rwb')
            def call(cmd,**kw):
                request=dict(cmd=cmd,**kw)
                stream.write((json.dumps(request)+'\n').encode());stream.flush()
                response=json.loads(stream.readline())
                if not response.get('ok'): raise RuntimeError(str(response))
                records.append(dict(request=request,response={k:v for k,v in response.items() if k!='data'}))
                return response
            call('savestate_load',path=str(state))
            for i in range(steps):
                call('set_keyinput',value=keys if i<steps-10 else 1023)
                frame=call('step')['frame']
                native=call('screenshot');host=call('host_screenshot')
                a=bytes.fromhex(native['data']);b=bytes.fromhex(host['data'])
                assert (native['w'],native['h'])==(240,160)
                assert (host['w'],host['h'])==(width,160)
                left=(width-240)//2
                assert b''.join(b[(y*width+left)*3:(y*width+left+240)*3] for y in range(160))==a
                pictures.append((frame,a,b))
                hashes.append(call('state_hash'))
                if i in (1,steps//2,steps-1): png(out/f'frame-{frame}.png',width,160,b)
            call('set_keyinput',value=1023)
            for region,length in (('ewram',0x40000),('iwram',0x8000),('oam',0x400),('io',0x400),('vram',0x18000),('pal',0x400)):
                snapshot=call('read_'+region,addr=0,len=length)
                (out/(region+'.bin')).write_bytes(bytes.fromhex(snapshot['data']))
            call('quit');stream.close();connection.close();connection=None
            process.wait(timeout=15)
            assert process.returncode==0
        finally:
            if connection: connection.close()
            if process.poll() is None: process.terminate();process.wait(timeout=10)
            (out/'commands.json').write_text(json.dumps(records,indent=2))
    return pictures,hashes

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--state',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
    p.add_argument('--width',type=int,choices=(284,320,384),default=384)
    p.add_argument('--steps',type=int,default=60);p.add_argument('--keys',type=lambda s:int(s,0),default=1023)
    p.add_argument('--expected-last-host',help='SHA256 of a separately reviewed final RGB host image, including margins')
    p.add_argument('--feature',choices=('objects','battles','replay'),default='objects',help='Feature to compare disabled/enabled')
    a=p.parse_args();out=a.output_dir.resolve();state=a.state.resolve()
    assert out.is_relative_to(ROOT/'validation') and 3<=a.steps<=300
    out.mkdir(parents=True,exist_ok=False)
    digest=hashlib.sha256(state.read_bytes()).hexdigest()
    before,bhash=run(state,out/'before',a.width,0,a.steps,a.keys,a.feature)
    after,ahash=run(state,out/'after',a.width,1,a.steps,a.keys,a.feature)
    changed=[f for (f,n,h),(g,m,j) in zip(before,after) if h!=j]
    center_changed=[f for (f,n,h),(g,m,j) in zip(before,after) if f!=g or n!=m]
    e0=(out/'before/ewram.bin').read_bytes();e1=(out/'after/ewram.bin').read_bytes()
    iw=(out/'after/iwram.bin').read_bytes()
    field=int.from_bytes(iw[0x6b54:0x6b58],'little')-0x02000000
    differences=[i for i,(b,c) in enumerate(zip(e0,e1)) if b!=c]
    def draw_storage(i):
        npc=i-field-0xab8; entity=i-field-0x1538
        return (0<=npc<32*0x54 and npc%0x54>=0x1c) or (0<=entity<32*0x3c and entity%0x3c>=0x14)
    report=dict(frames=len(after),width=a.width,feature=a.feature,changed_margin_frames=changed,native_difference_frames=center_changed,
        last_host_rgb_sha256=hashlib.sha256(after[-1][2]).hexdigest(),
        visual_reference_checked=bool(a.expected_last_host),
        region_difference_counts={key:sum(b[key]!=c[key] for b,c in zip(bhash,ahash)) for key in ('ewram','iwram','vram','pal','oam','cycles')},
        final_ewram_difference_offsets=[hex(i) for i in differences],
        final_ewram_non_draw_differences=[hex(i) for i in differences if not draw_storage(i)],
        source_unchanged=digest==hashlib.sha256(state.read_bytes()).hexdigest(),
        executable_sha256=hashlib.sha256((ROOT/'build-native/Swordcraft3CustomRendererBeta.exe').read_bytes()).hexdigest())
    (out/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
    assert report['source_unchanged'] and not center_changed
    if a.feature=='replay':
        assert not changed, 'Removing native replay changed the complete host picture'
        assert bhash==ahash, 'Removing native replay changed guest state'
    if a.expected_last_host:
        assert report['last_host_rgb_sha256']==a.expected_last_host, 'Offscreen-inclusive visual regression'

if __name__=='__main__': main()
