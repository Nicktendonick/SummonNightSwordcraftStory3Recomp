// Verify GBA mapping and export focused decompilations into PRIVATE evidence.
// @category Swordcraft3
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.Function;
import com.google.gson.*;
import java.nio.file.*;
import java.util.*;

public class VerifySc3 extends GhidraScript {
    public void run() throws Exception {
        Path out = Path.of(getScriptArgs()[0]);
        Files.createDirectories(out);
        if (!currentProgram.getLanguageID().toString().equals("ARM:LE:32:v4t"))
            throw new IllegalStateException("Wrong processor: " + currentProgram.getLanguageID());
        if (!currentProgram.getExecutableFormat().equals("GBA Loader"))
            throw new IllegalStateException("GBA loader was not selected");
        for (long a : new long[]{0x02000000L,0x03000000L,0x04000000L,0x05000000L,
                                 0x06000000L,0x07000000L,0x08000000L})
            if (currentProgram.getMemory().getBlock(toAddr(a)) == null)
                throw new IllegalStateException("Missing GBA region: " + Long.toHexString(a));
        Map<String,Object> report = new LinkedHashMap<>();
        report.put("program", currentProgram.getName());
        report.put("sha256", currentProgram.getExecutableSHA256());
        report.put("language", currentProgram.getLanguageID().toString());
        report.put("loader", currentProgram.getExecutableFormat());
        report.put("function_count", currentProgram.getFunctionManager().getFunctionCount());
        List<Map<String,Object>> checks = new ArrayList<>();
        DecompInterface dc = new DecompInterface();
        dc.openProgram(currentProgram);
        try {
            for (long a : new long[]{0x08093994L,0x08094A4CL,0x08001D3CL,0x08001D78L,0x08031BC8L}) {
                Function f = getFunctionAt(toAddr(a));
                if (f == null) throw new IllegalStateException("Missing focus function " + Long.toHexString(a));
                DecompileResults r = dc.decompileFunction(f,90,monitor);
                if (!r.decompileCompleted()) throw new IllegalStateException(r.getErrorMessage());
                Files.writeString(out.resolve(String.format("%08x.c",a)),r.getDecompiledFunction().getC());
                checks.add(Map.of("address",String.format("%08x",a),"name",f.getName(),
                    "decompiled",true,"callers",f.getCallingFunctions(monitor).size()));
            }
        } finally { dc.dispose(); }
        report.put("focus_checks",checks);
        report.put("warning","Pseudocode is inferred, not original source; verify with assembly and runtime state.");
        Files.writeString(out.resolve("verification.json"),new GsonBuilder().setPrettyPrinting().create().toJson(report));
        println("SC3_VERIFY_PASS " + report.get("function_count") + " analyzed functions, five decompilation checks.");
    }
}
