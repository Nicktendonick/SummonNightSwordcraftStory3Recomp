// Import address/mode seeds checked against the exact input ROM.
// @category Swordcraft3
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.SourceType;
import java.math.BigInteger;
import java.nio.file.*;

public class SeedSc3 extends GhidraScript {
    public void run() throws Exception {
        int seeded = 0;
        for (String line : Files.readAllLines(Path.of(getScriptArgs()[0]))) {
            monitor.checkCancelled();
            if (line.startsWith("#") || line.isBlank()) continue;
            String[] p = line.split("\t");
            Address a = toAddr(Long.decode(p[0]));
            if (a.getOffset() < 0x080000C0L) continue;
            currentProgram.getProgramContext().setValue(currentProgram.getRegister("TMode"), a, a,
                p[1].equals("thumb") ? BigInteger.ONE : BigInteger.ZERO);
            currentProgram.getSymbolTable().createLabel(a, p[2], SourceType.IMPORTED);
            currentProgram.getSymbolTable().addExternalEntryPoint(a);
            seeded++;
        }
        println("SC3 imported " + seeded + " ROM-checked seeds; not a complete function inventory.");
    }
}
