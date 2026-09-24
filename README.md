# logic_sim

Event-driven, 4-state (0 / 1 / X / Z) digital logic simulator in C++17,
built as the back end for a GUI logic simulator.

Logic simulator by:
- Nasser Alahaideb
- Sebastian Paulus
- Yonathan Siele

## Building

Needs CMake 3.20+ and a C++17 compiler.

```sh
make            # Debug build (asserts + AddressSanitizer/UBSan) in build/Debug
make run        # build and run the demo (full adder table + 4-bit counter)
make test       # build and run the unit tests
make release    # optimised build and demo run
make doxygen    # API reference PDF from the code comments: build/doxygen/logicsim.pdf
./build/Debug/logicsim wave.vcd   # demo, also writes a waveform for GTKWave
```

`make doxygen` needs Doxygen, pdflatex and the ~40 LaTeX packages Doxygen's
output uses. Installing the TeX Live collections that hold them avoids
chasing missing `.sty` files one at a time:

```sh
# Fedora
sudo dnf install doxygen ghostscript texlive-collection-latexrecommended \
    texlive-collection-latexextra texlive-collection-fontsrecommended \
    texlive-collection-plaingeneric
# Debian / Ubuntu
sudo apt install doxygen ghostscript texlive-latex-recommended \
    texlive-latex-extra texlive-fonts-recommended texlive-plain-generic
```

Ghostscript adds class diagrams; without it the PDF is built without them. Settings are in `Doxyfile`. If LaTeX stops
on a missing package (`File 'x.sty' not found`), `make doxygen` prints it;
on Fedora install it with `sudo dnf install 'tex(x.sty)'`.

Or with CMake directly:

```sh
cmake -B build/Debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
ctest --test-dir build/Debug --output-on-failure
```

The engine builds as a static library, `logicsim_core`, which a GUI links
against. Options: `-DLOGICSIM_BUILD_TESTS=OFF`, `-DLOGICSIM_SANITIZE=OFF`.

## Quick example

```cpp
#include "Simulator.h"

Simulator sim;                                  // unit-delay timing by default
Net& a = sim.addNet("a");
Net& b = sim.addNet("b");
Net& y = sim.addNet("y");
Input& ia = sim.addInput(&a, "A");             // switches / input pins
Input& ib = sim.addInput(&b, "B");
sim.addGate(BuiltinGate::Type::NAND, {&a, &b}, {&y});

sim.setInput(ia, LogicValue::HIGH);
sim.setInput(ib, LogicValue::HIGH);
sim.runUntilIdle();                             // propagate until quiet
// y.getValue() == LogicValue::LOW
```

See `src/main.cpp` for modules, clocks and flip-flops, and `tests/` for
every feature in use.

## Architecture

| File | Role |
|---|---|
| `Types.h` | `LogicValue` (0/1/X/Z) and its operators, driver `Strength`, helpers |
| `Event.h`, `EventQueue.h` | Timing wheel + overflow heap; Active / Update / Monitor regions; delta-cycle oscillation guard |
| `Net.h` | A wire. Resolves multiple drivers by strength (tri-state buses, pullups), force/release, change tracking |
| `Gate.h` | Base class for every component: inertial-delay scheduling, rewiring, timing models |
| `BuiltinGate.h` | and/nand/or/nor/xor/xnor, buf/not, bufif0/1, notif0/1, pullup/pulldown |
| `FlipFlop.h`, `Latch.h` | D/T/JK/SR edge-triggered flip-flops (async set/reset), gated D latch |
| `Input.h`, `Clock.h` | Stimulus sources: externally set values, free-running clock |
| `Module.h` | Reusable sub-circuit blueprints with named ports, nestable |
| `Simulator.h` | Owns the netlist, runs it, handles edits, notifies listeners. **The class a GUI talks to** |
| `VcdWriter.h` | Waveform output (VCD) |

### Simulation semantics

- **Values.** A floating (Z) gate input reads as X. Only enable gates
  (`bufif`/`notif`) can put Z on a net.
- **Nets.** Released (Z) drivers are ignored. Of the rest, only the strongest
  count (`Strong` gate outputs beat `Pull` resistors). If they disagree the net
  is X, and `Net::hasConflict()` reports the bus fight. With no active driver
  the net floats at Z.
- **Delay.** Inertial: a pulse shorter than a gate's delay is swallowed.
  Rise, fall and turn-off (decay) delays are separate. A `TimingModel` sets
  them (`ZeroDelayModel`, `UnitDelayModel` (default), `StageDelayModel`,
  which is stage count plus fanout). `Gate::setFixedDelays()` pins one gate.
- **Regions.** Within a timestep, gate outputs commit in *Active*,
  flip-flop and latch outputs in *Update*, and read-only work in *Monitor*.
  A later region only runs once every earlier one is empty, so all
  flip-flops on a clock edge sample before any of them change, even with
  zero delay and clock skew.
- **Oscillation.** A timestep that never settles (for example a zero-delay
  ring of inverters) stops `run()` with `Result::Oscillation`, and
  `oscillatingGates()` lists the culprits.
- **Sequential start-up.** Flip-flops and latches power up to 0 (configurable
  with `setInitialState`). Clock transitions from or to Z are not edges, so an
  undriven clock at start-up never triggers anything. A clock that passes
  through X makes the state X unless the edge could not have changed it.
- **Time.** After `run(t)` the time is exactly `t`. `step()` jumps to the next
  event. `runUntilIdle()` runs until nothing is pending.

### Editing while simulating

The netlist can be changed at any time: `addGate`, `removeGate`,
`removeNet`, `connectInput` / `connectOutput`, `mergeNets`,
`instantiate` / `removeInstance`. An edit marks the circuit dirty. Before
time next moves, the simulator re-applies the timing model, re-resolves
every net and re-evaluates every gate at the current time, so it picks up
where it was. Removing a gate also drops its pending events.

Every net and gate has a numeric `id()` that is never reused
(`sim.net(id)`, `sim.gate(id)`). That's the handle to pass across an API
boundary. Pins may be left unconnected (`nullptr`).

### Observing

`addChangeListener()` is called once per net whose settled value changed in
a timestep. Glitches that come and go inside one timestep are filtered out,
so a GUI can redraw only the wires that changed. `VcdWriter` uses the same
hook.

## Towards the GUI API

The engine side is ready for a front end:

- all state is reachable by id; creation, deletion and rewiring are cheap
  and safe mid-run;
- `setInput`, `setClockRunning`, `forceNet` / `releaseNet` for interaction;
- `run` / `runFor` / `step` / `runUntilIdle` / `reset` for transport controls;
- change listeners for incremental redraws, `oscillatingGates()` for errors.

Still to decide / build for the API layer:

- **Threading.** `Simulator` is not thread-safe. Run it on one thread and
  post GUI edits to that thread, or guard every call with a mutex.
- **Multi-bit buses.** Every net is one bit. Buses (and splitters, registers,
  RAM/ROM, adders as primitives) would be the next big engine feature.
- **Save/load format** (e.g. JSON): `BuiltinGate::typeFromName()` and the
  fixed per-type pin layouts are there to make this mechanical.
- **Error reporting across the boundary.** Invalid requests throw
  `std::invalid_argument` / `std::out_of_range`. A C API or bindings layer
  should catch and convert them.
