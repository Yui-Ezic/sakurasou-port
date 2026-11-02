import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
public class DumpAll extends GhidraScript {
  public void run() throws Exception {
    String out = getScriptArgs()[0];
    DecompInterface d = new DecompInterface();
    DecompileOptions o = new DecompileOptions();
    d.setOptions(o); d.openProgram(currentProgram);
    PrintWriter w = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(out), "UTF-8")));
    int n=0; println("functions: " + currentProgram.getFunctionManager().getFunctionCount());
    for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
      if (monitor.isCancelled()) break;
      DecompileResults r = d.decompileFunction(f, 60, monitor);
      w.println("//@@ " + f.getEntryPoint() + " " + f.getName() + " size=" + f.getBody().getNumAddresses());
      if (r != null && r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
      else w.println("// decompile failed");
      if (++n % 200 == 0) { w.flush(); println("done " + n); }
    }
    w.close();
  }
}
