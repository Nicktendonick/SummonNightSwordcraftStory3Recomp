// Serve the installed upstream MCP against a headless, read-only database.
// @category Swordcraft3
import ghidra.app.script.GhidraScript;
import ghidra.mcp.MCPServer;
import java.nio.file.*;

public class ServeSc3Mcp extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String key = System.getenv("GHIDRA_API_KEY");
        if (key == null || key.length() < 32) throw new IllegalStateException("Authentication required");
        long parent = Long.parseLong(args[3]);
        MCPServer server = new MCPServer(null);
        server.setPort(Integer.parseInt(args[0]));
        server.setRestrictToLocalhost(true);
        server.setApiKey(key);
        server.setCurrentProgram(currentProgram);
        try {
            server.startServer();
            if (!server.isRunning()) throw new IllegalStateException("Local MCP failed to start");
            Files.writeString(Path.of(args[1]),currentProgram.getName());
            while (!Files.exists(Path.of(args[2])) &&
                   ProcessHandle.of(parent).map(ProcessHandle::isAlive).orElse(false)) {
                monitor.checkCancelled();
                Thread.sleep(500);
            }
        } finally { server.stopServer(); }
    }
}
