# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The machine-code arm of `harness.py prove`: a compiled proof harness run symbolically from main.

Run with angr's interpreter: `symbolic.py <program> [--steps N]`. nondet_size and nondet_byte answer fresh symbols,
prove_assume adds its argument's truth to the path's constraints, and prove_failed ends the path as a counterexample.
Every path is explored to its end. The answer is one line: `proved` where no satisfiable path reaches prove_failed
and every path ended and at least one satisfiable path reached prove_reached, `counterexample <what> <inputs>`
where one reaches prove_failed, or `incomplete <reason>` where the exploration hit a path it could not finish or no
path reached the end, which is never read as a proof.
"""

import sys

import angr

SIZE_BITS = 64


class SizeInput(angr.SimProcedure):
    def run(self):
        value = self.state.solver.BVS("size", SIZE_BITS)
        self.state.globals["inputs"] = self.state.globals.get("inputs", ()) + (value,)
        return value


class ByteInput(angr.SimProcedure):
    def run(self):
        value = self.state.solver.BVS("byte", 8).zero_extend(SIZE_BITS - 8)
        self.state.globals["inputs"] = self.state.globals.get("inputs", ()) + (value,)
        return value


class Assume(angr.SimProcedure):
    def run(self, holds):
        self.state.add_constraints(holds[31:0] != 0)
        if not self.state.satisfiable():
            self.exit(0)


class ClockRead(angr.SimProcedure):
    """clock_gettime of any clock: the seconds and nanoseconds fresh symbols, so a proof holds for every reading."""

    def run(self, clock, spec):
        self.state.memory.store(spec, self.state.solver.BVS("clock_s", SIZE_BITS), endness=self.arch.memory_endness)
        nanoseconds = self.state.solver.BVS("clock_ns", SIZE_BITS)
        self.state.add_constraints(nanoseconds < 1000000000)
        self.state.memory.store(spec + 8, nanoseconds, endness=self.arch.memory_endness)
        return 0


class Reached(angr.SimProcedure):
    def run(self):
        self.state.globals["reached"] = True


class Failed(angr.SimProcedure):
    def run(self, what):
        self.state.globals["failed"] = self.state.mem[what].string.concrete.decode("utf-8", "replace")
        self.exit(1)


def main(argv):
    program = argv[1]
    steps = int(argv[argv.index("--steps") + 1]) if "--steps" in argv else 200000
    project = angr.Project(program, auto_load_libs=False)
    for name, procedure in (("nondet_size", SizeInput), ("nondet_byte", ByteInput), ("prove_assume", Assume),
                            ("prove_failed", Failed), ("prove_reached", Reached)):
        if project.loader.find_symbol(name) is None:
            print("incomplete the program has no %s" % name)
            return 2
        project.hook_symbol(name, procedure())
    project.hook_symbol("clock_gettime", ClockRead(), replace=True)
    state =project.factory.entry_state(add_options={angr.options.ZERO_FILL_UNCONSTRAINED_MEMORY,
                                                     angr.options.ZERO_FILL_UNCONSTRAINED_REGISTERS})
    manager = project.factory.simulation_manager(state)
    taken = 0
    while manager.active and taken < steps:
        manager.step()
        taken += 1
    for found in manager.deadended:
        if "failed" in found.globals and found.satisfiable():
            inputs = [found.solver.eval(value) for value in found.globals.get("inputs", [])]
            print("counterexample %s %s" % (found.globals["failed"], inputs))
            return 1
    if manager.active:
        print("incomplete %d paths still running after %d steps" % (len(manager.active), taken))
        return 2
    if manager.errored:
        print("incomplete %d paths errored: %s" % (len(manager.errored), manager.errored[0].error))
        return 2
    reached = [found for found in manager.deadended if found.globals.get("reached") and found.satisfiable()]
    if not reached:
        print("incomplete no path reaches the end of the harness: its assumptions leave nothing to prove")
        return 2
    print("proved %d paths, %d reaching the end" % (len(manager.deadended), len(reached)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
