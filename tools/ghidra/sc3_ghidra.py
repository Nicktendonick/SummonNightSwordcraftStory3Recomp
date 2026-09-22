"""Local Ghidra analysis and read-only MCP, using the installed upstream bridge.

No changes to ROMs, saves, generated guest code or game presentation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import sys
import threading
import time
import uuid

SCRIPTS = Path(__file__).resolve().parent
PROJECT = next(p for p in SCRIPTS.parents if (p / ".tooling/ghidra").is_dir())
TOOLS = PROJECT / ".tooling/ghidra"
INSTALL = TOOLS / "ghidra_12.0.4_PUBLIC"
JAVA = TOOLS / "jdk-21.0.12.1+1/bin/java.exe"
PRIVATE = PROJECT / "validation/ghidra"
BRIDGE = INSTALL / "Ghidra/Extensions/GhidraMCP/data/os/win_x86_64/mcp_bridge.exe"
ROM = {"jp": PROJECT / "roms/swordcraft3_jp.gba",
       "beta": PROJECT / "build-beta/rom-patch-cache/swordcraft3_beta.gba"}
READ_TOOLS = frozenset({
    "list_functions", "list_imports", "list_exports", "list_namespaces", "list_data_items",
    "list_strings", "search_functions_by_name", "get_function_by_address",
    "decompile_function", "decompile_function_by_address", "disassemble_function",
    "get_xrefs_to", "get_xrefs_from", "get_function_xrefs", "get_program_info",
    "get_memory_map", "get_variables", "get_basic_blocks", "search_bytes",
    "get_bookmarks", "list_equates", "get_structure", "list_structures",
    "decompile_function_async", "get_decompile_result", "ping"})


def command(gui=False):
    # Mirrors support/launch.bat without a global Java install or user settings.
    args = [str(JAVA), "-Xmx2G", "-Xshare:off", "-XX:ParallelGCThreads=2",
            "-XX:CICompilerCount=2", "-Djava.system.class.loader=ghidra.GhidraClassLoader",
            "-Dfile.encoding=UTF8", "-Duser.country=US", "-Duser.language=en",
            "-Dsun.java2d.d3d=false", "-Dlog4j.skipJansi=true", "-Dcpu.core.limit=2",
            "--enable-native-access=ALL-UNNAMED"]
    for prop, folder in [("application.settingsdir", "settings"),
                         ("application.cachedir", "cache"),
                         ("application.tempdir", "tmp"), ("java.io.tmpdir", "tmp")]:
        args.append(f"-D{prop}={TOOLS / folder}")
    args += ["-cp", str(INSTALL / "Ghidra/Framework/Utility/lib/Utility.jar"),
             "ghidra.Ghidra", "ghidra.GhidraRun" if gui else "ghidra.app.util.headless.AnalyzeHeadless"]
    return args


def project_args(variant):
    return [str(PRIVATE / "projects"), "Swordcraft3_" + variant]


def prepare_seeds(variant):
    jp = ROM["jp"].read_bytes()
    if hashlib.sha1(jp).hexdigest() != "3f5253fcf57e07ce52472bd29a61d16b98a12376":
        raise RuntimeError("Original ROM does not match csm3's recorded SHA1")
    data = ROM[variant].read_bytes()
    entries = []
    for line in (PROJECT / "symbols/imported_symbols.tsv").read_text().splitlines():
        if not line.startswith("#") and line.strip():
            address, mode, name = line.split("\t")
            if 0x080000C0 <= int(address,16) < 0x080B70C4:
                entries.append((int(address,16),mode,name))
    entries.sort()
    accepted, skipped = [], []
    for i, (addr,mode,name) in enumerate(entries):
        end = entries[i+1][0] if i+1 < len(entries) else 0x080B70C4
        lo,hi = addr-0x08000000,end-0x08000000
        # Require entire seed interval to match, not merely the function prologue.
        if variant == "jp" or (hi <= len(data) and jp[lo:hi] == data[lo:hi]):
            accepted.append(f"0x{addr:08X}\t{mode}\t{name}")
        else: skipped.append(f"0x{addr:08X}")
    out = PRIVATE / variant
    out.mkdir(parents=True,exist_ok=True)
    seedfile = out / "checked-seeds.tsv"
    seedfile.write_text("# ROM-checked csm3 address/mode seeds\n"+"\n".join(accepted)+"\n")
    (out/"input.json").write_text(json.dumps({"variant":variant,"rom":str(ROM[variant]),
        "sha256":hashlib.sha256(data).hexdigest(),"seed_count":len(accepted),
        "skipped_changed_intervals":skipped,"last_unbounded_symbol_omitted":"080b70c4"},indent=2))
    return seedfile,out


def analysis_status(variant):
    out = PRIVATE / variant
    report = json.loads((out/"verification.json").read_text())
    log = (out/"analysis.log").read_text(encoding="utf-8",errors="replace")
    report["analysis_timed_out"] = "Analysis timed out" in log
    report["complete_game_decompilation"] = False
    report["function_count_is_authoritative"] = False
    (out/"verification.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps(report,indent=2))


def analyze(variant):
    (PRIVATE/"projects").mkdir(parents=True,exist_ok=True)
    if (PRIVATE/"projects"/f"Swordcraft3_{variant}.gpr").exists():
        raise RuntimeError("Analysis project already exists; refusing to overwrite it")
    seeds,out = prepare_seeds(variant)
    args = command()+project_args(variant)+["-import",str(ROM[variant]),
        "-loader","GBALoader","-scriptPath",str(SCRIPTS),
        "-preScript","SeedSc3.java",str(seeds),
        "-postScript","VerifySc3.java",str(out),"-max-cpu","2",
        "-analysisTimeoutPerFile","900","-log",str(out/"analysis.log")]
    result = subprocess.run(args,creationflags=subprocess.CREATE_NO_WINDOW,
                            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding="utf-8",errors="replace")
    (out/"console.log").write_text(result.stdout,encoding="utf-8")
    if result.returncode or not (out/"verification.json").exists():
        print(result.stdout[-10000:])
        raise RuntimeError(f"Analysis exit {result.returncode}; see {out}")
    analysis_status(variant)


def mcp_live(variant):
    if not (PRIVATE/variant/"verification.json").exists():
        raise RuntimeError("Run and verify analysis first")
    session = PRIVATE/"sessions"/uuid.uuid4().hex
    session.mkdir(parents=True)
    ready,stop = session/"ready",session/"stop"
    with socket.socket() as sock:
        sock.bind(("127.0.0.1",0))
        port = sock.getsockname()[1]
    env = dict(os.environ,GHIDRA_API_KEY=secrets.token_hex(32))
    args = command()+project_args(variant)+["-process",ROM[variant].name,
        "-readOnly","-noanalysis","-scriptPath",str(SCRIPTS),
        "-postScript","ServeSc3Mcp.java",str(port),str(ready),str(stop),str(os.getpid()),
        "-log",str(session/"ghidra.log")]
    upstream = None
    with (session/"server.log").open("w") as logfile:
        server = subprocess.Popen(args,env=env,stdout=logfile,stderr=logfile,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic()+100
            while not ready.exists():
                if server.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError(f"Ghidra MCP startup failed; see {session}")
                time.sleep(.2)
            upstream = subprocess.Popen([str(BRIDGE),"--host","127.0.0.1","--port",str(port)],
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=sys.stderr,
                env=env,text=True,encoding="utf-8",creationflags=subprocess.CREATE_NO_WINDOW)
            output_lock = threading.Lock()

            def emit(message):
                with output_lock: print(json.dumps(message),flush=True)

            def receive():
                for line in upstream.stdout:
                    message = json.loads(line)
                    result = message.get("result")
                    if isinstance(result,dict) and "tools" in result:
                        result["tools"] = [t for t in result["tools"] if t["name"] in READ_TOOLS]
                    emit(message)

            thread = threading.Thread(target=receive,daemon=True)
            thread.start()
            for line in sys.stdin:
                message = json.loads(line)
                params = message.get("params",{})
                if message.get("method")=="tools/call" and (
                    params.get("name") not in READ_TOOLS or "target_port" in params.get("arguments",{})):
                    emit({"jsonrpc":"2.0","id":message.get("id"),"error":{
                        "code":-32601,"message":"Only read-only tools on this Swordcraft analysis are allowed"}})
                    continue
                upstream.stdin.write(line)
                upstream.stdin.flush()
            upstream.stdin.close()
            upstream.wait(timeout=10)
            thread.join(timeout=5)
        finally:
            if upstream is not None and upstream.poll() is None:
                upstream.terminate()
                upstream.wait(timeout=10)
            stop.touch()
            try: server.wait(timeout=20)
            except subprocess.TimeoutExpired:
                server.terminate()
                server.wait(timeout=10)


def mcp(variant):
    # Publish the verified tool schema immediately, but open Ghidra only on the
    # first actual query. A registered connection must not start a 2GB Java VM
    # in every unrelated task. The bridge is still the live query backend.
    schema = TOOLS / "readonly-tools.json"
    if not schema.exists():
        return mcp_live(variant)  # Initial installation/verification only.
    live = None
    sequence = 100000
    def emit(message):
        print(json.dumps(message),flush=True)
    def exchange(method,params):
        nonlocal sequence
        sequence += 1
        live.stdin.write(json.dumps({"jsonrpc":"2.0","id":sequence,"method":method,"params":params})+"\n")
        live.stdin.flush()
        for line in live.stdout:
            result=json.loads(line)
            if result.get("id")==sequence: return result
        raise RuntimeError("Live Ghidra connection closed; inspect validation/ghidra/sessions")
    try:
        for line in sys.stdin:
            message=json.loads(line)
            method=message.get("method")
            if "id" not in message: continue
            response={"jsonrpc":"2.0","id":message["id"]}
            params=message.get("params",{})
            if method=="initialize":
                response["result"]={"protocolVersion":"2024-11-05","capabilities":{"tools":{}},
                    "serverInfo":{"name":"Swordcraft3-readonly-Ghidra","version":"1.0"},
                    "instructions":f"Read-only Swordcraft Story 3 {variant} ROM analysis. Pseudocode is inferred; verify against assembly and live game-state traces. No ROM patching or other-project access."}
            elif method=="tools/list":
                response["result"]={"tools":json.loads(schema.read_text())}
            elif method=="ping":
                response["result"]={}
            elif method=="tools/call" and params.get("name") in READ_TOOLS and "target_port" not in params.get("arguments",{}):
                try:
                    if live is None:
                        live=subprocess.Popen([sys.executable,"-B",str(Path(__file__)),"mcp-live","--variant",variant],
                            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=sys.stderr,
                            text=True,encoding="utf-8",creationflags=subprocess.CREATE_NO_WINDOW)
                        init=exchange("initialize",{"protocolVersion":"2024-11-05","capabilities":{},
                            "clientInfo":{"name":"Swordcraft3-readonly-proxy","version":"1"}})
                        if "error" in init: raise RuntimeError(str(init["error"]))
                        live.stdin.write('{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
                        live.stdin.flush()
                    response=exchange(method,params)
                    response["id"]=message["id"]
                except Exception as error:
                    response["result"]={"isError":True,"content":[{"type":"text","text":str(error)}]}
            else:
                response["error"]={"code":-32601,"message":"Only read-only tools on this Swordcraft analysis are allowed"}
            emit(response)
    finally:
        if live is not None:
            live.stdin.close()
            try: live.wait(timeout=35)
            except subprocess.TimeoutExpired:
                live.terminate()
                live.wait(timeout=10)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("action",choices=["analyze","status","gui","mcp","mcp-live"])
    parser.add_argument("--variant",choices=["jp","beta"],default="beta")
    opts=parser.parse_args()
    if opts.action=="analyze": analyze(opts.variant)
    elif opts.action=="status": analysis_status(opts.variant)
    elif opts.action=="mcp": mcp(opts.variant)
    elif opts.action=="mcp-live": mcp_live(opts.variant)
    else:
        project=PRIVATE/"projects"/f"Swordcraft3_{opts.variant}.gpr"
        if not project.exists(): raise RuntimeError("Analysis project has not been created")
        with (PRIVATE/f"gui-{opts.variant}.log").open("a") as log:
            subprocess.Popen(command(gui=True)+[str(project)],stdout=log,stderr=log,
                             creationflags=subprocess.CREATE_NO_WINDOW)


if __name__=="__main__": main()
