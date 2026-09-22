"""End-to-end initialize/list/query/decompile/read-only boundary test."""
import argparse
import hashlib
import json
from pathlib import Path
import queue
import subprocess
import sys
import threading
from sc3_ghidra import PRIVATE, READ_TOOLS, ROM, SCRIPTS, TOOLS

parser=argparse.ArgumentParser()
parser.add_argument("--variant",choices=["jp","beta"],default="beta")
args=parser.parse_args()
out=PRIVATE/args.variant
database=PRIVATE/"projects"/f"Swordcraft3_{args.variant}.rep"
def database_hashes():
    return {str(p.relative_to(database)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in database.rglob("*.gbf")}
before=database_hashes()
assert before, f"No database storage found in {database}"
with (out/"mcp-test-stderr.log").open("w") as log:
    proc=subprocess.Popen([sys.executable,"-B",str(SCRIPTS/"sc3_ghidra.py"),"mcp","--variant",args.variant],
        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,text=True,encoding="utf-8",
        creationflags=subprocess.CREATE_NO_WINDOW)
    messages=queue.Queue()
    def reader():
        for line in proc.stdout: messages.put(json.loads(line))
        messages.put(None)
    threading.Thread(target=reader,daemon=True).start()
    counter=0
    evidence={}
    def request(method,params):
        global counter
        counter+=1
        proc.stdin.write(json.dumps({"jsonrpc":"2.0","id":counter,"method":method,"params":params})+"\n")
        proc.stdin.flush()
        while True:
            response=messages.get(timeout=150)
            if response is None: raise RuntimeError(f"MCP exited; see {out/'mcp-test-stderr.log'}")
            if response.get("id")==counter: return response
    try:
        evidence["initialize"]=request("initialize",{"protocolVersion":"2024-11-05",
            "capabilities":{},"clientInfo":{"name":"sc3-local-verifier","version":"1"}})
        assert "result" in evidence["initialize"]
        proc.stdin.write('{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
        proc.stdin.flush()
        listed=request("tools/list",{})
        names={t["name"] for t in listed["result"]["tools"]}
        assert names and names <= READ_TOOLS and "decompile_function_by_address" in names
        if not (TOOLS/"readonly-tools.json").exists():
            (TOOLS/"readonly-tools.json").write_text(json.dumps(listed["result"]["tools"],indent=2),encoding="utf-8")
        evidence["tools"]=sorted(names)
        info=request("tools/call",{"name":"get_program_info","arguments":{}})
        assert not info["result"].get("isError"), info
        text=info["result"]["content"][0]["text"]
        assert ROM[args.variant].name in text and "ARM:LE:32:v4t" in text, text
        evidence["program_info"]=info
        for address in ["08093994","08094a4c"]:
            result=request("tools/call",{"name":"decompile_function_by_address","arguments":{"address":address}})
            assert not result["result"].get("isError"), result
            text=result["result"]["content"][0]["text"]
            assert "Error:" not in text and "{" in text and "}" in text, text
            evidence[address]=result
        for name,arguments in [("patch_bytes",{}),("get_program_info",{"target_port":8765})]:
            result=request("tools/call",{"name":name,"arguments":arguments})
            assert result.get("error",{}).get("code")==-32601, result
        evidence["write_and_port_override_rejected"]=True
        evidence["status"]="PASS"
    finally:
        proc.stdin.close()
        try: proc.wait(timeout=35)
        except subprocess.TimeoutExpired:
            proc.terminate()
            proc.wait(timeout=10)
    assert proc.returncode==0,proc.returncode
    assert database_hashes()==before, "Read-only session changed database storage"
    evidence["database_storage_unchanged"]=True
    (out/"mcp-verification.json").write_text(json.dumps(evidence,indent=2),encoding="utf-8")
    print(f"PASS {args.variant}: {len(names)} read-only tools, correct ROM, two live decompilations, writes blocked, clean shutdown")
