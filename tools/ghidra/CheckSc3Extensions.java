// Check extension linkage without a ROM or a listening server.
// @category Swordcraft3
import ghidra.app.script.GhidraScript;
import ghidra.mcp.MCPServer;
import gba.GBALoader;

public class CheckSc3Extensions extends GhidraScript {
    public void run() throws Exception {
        if (!new GBALoader().getName().equals("GBA Loader")) throw new IllegalStateException("Wrong loader");
        MCPServer server = new MCPServer(null);
        if (server.isRunning()) throw new IllegalStateException("Unexpected listening server");
        server.stopServer();
        println("SC3_EXTENSIONS_PASS: GBA loader and MCP classes available headlessly");
    }
}
