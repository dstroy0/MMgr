// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
// negotiated commercial licensing contract or an educator's license issued to you personally.
//
// The postScript harness.py verify hands Ghidra's headless analyzer: every function of the program it analyzed,
// decompiled into <out>/<function>.c, and one line each in <out>/functions.tsv: the name, the entry, the bytes,
// the instructions and the functions it calls, sorted. The script argument is <out>.
// @category MMgr

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class DecompileAll extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : "decompiled");
        out.mkdirs();

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        try (PrintWriter summary = new PrintWriter(new File(out, "functions.tsv"), StandardCharsets.UTF_8)) {
            summary.println("function\tentry\tbytes\tinstructions\tcalls");
            for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
                if (function.isExternal() || function.isThunk()) {
                    continue;
                }
                int instructions = 0;
                InstructionIterator walk = currentProgram.getListing().getInstructions(function.getBody(), true);
                while (walk.hasNext()) {
                    Instruction instruction = walk.next();
                    instructions += (instruction != null) ? 1 : 0;
                }
                List<String> calls = new ArrayList<>();
                for (Function called : function.getCalledFunctions(monitor)) {
                    calls.add(called.getName());
                }
                Collections.sort(calls);
                summary.println(function.getName() + "\t" + function.getEntryPoint() + "\t" +
                                function.getBody().getNumAddresses() + "\t" + instructions + "\t" +
                                String.join(",", calls));

                DecompileResults decompiled = decompiler.decompileFunction(function, 60, monitor);
                String text = decompiled.decompileCompleted() ? decompiled.getDecompiledFunction().getC()
                                                              : "// " + decompiled.getErrorMessage() + "\n";
                String file = function.getName().replaceAll("[^A-Za-z0-9_.]", "_") + ".c";
                try (PrintWriter source = new PrintWriter(new File(out, file), StandardCharsets.UTF_8)) {
                    source.print(text);
                }
            }
        }
        decompiler.dispose();
    }
}
