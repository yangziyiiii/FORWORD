# FORWORD – Word-Level Sweeping for Datapath Verification

FORWORD is a research prototype for accelerating formal datapath verification. It couples symbolic simulation, hashing-based equivalence detection, and SMT-based validation into a sweeping engine.

This document explains how to set up the environment, build the project, use the command-line tools, and integrate the sweeping API directly in your own workflows. It also outlines how to run the bundled benchmarks to evaluate FORWORD’s performance.

---

## 1. Prerequisites

Install the toolchain and system libraries (Ubuntu 20.04+ recommended):

```bash
sudo apt update
sudo apt install -y build-essential cmake default-jre libgmp-dev \
    libboost-all-dev libgflags-dev flex bison git
pip3 install --user toml
```

FORWORD relies on several third-party components (glog, btor2tools, smt-switch, etc.). Helper scripts in `contrib/` download and build tested versions of these dependencies.

---

## 2. Repository Layout

- `apps/` – Command-line frontends (e.g., `main_bmc.cpp`).
- `framework/` – Core transition-system utilities, traversals, e-graph support.
- `frontend/` – BTOR2 and SMT-LIB parsers/encoders.
- `sweeper/` – Simulation engine, node data tracking, and the sweeping algorithm.
- `contrib/` – Setup scripts for external projects.
- `benchmark/`

---

## 3. Setup & Build

1. **Clone** this repository and move into it.
2. **Fetch third-party dependencies** (run once per machine):

   ```bash
   ./contrib/setup-glog.sh
   ./contrib/setup-bison.sh
   ./contrib/setup-btor2tools.sh
   ./contrib/setup-smt-switch.sh
   ```

3. **Configure the CMake build**. The helper script accepts useful flags:

   ```bash
   ./configure.sh [--static-lib]
   ```

   Examples:
   - `./configure.sh --static-lib` – Produce a static `libwasim.a`.

4. **Build**:

   ```bash
   cd build
   make -j$(nproc)
   ```

   Successful builds generate `build/libwasim.a` and the example binaries (`build/bmc`, etc.).

---

## 4. Usage

### 4.1 Command-Line (BMC Frontend)

The `bmc` application demonstrates the sweeping flow on BTOR2 benchmarks.

```bash
./build/bmc \
  --file benchmark/mul/mul_1.btor2 \
  --iteration 100 \
  --bound 10 \
  --solver_timeout 600000 \
  --prop_timeout 600000 \
  --find_unsat 20 \
  --find_sat 1 \
  --debug
```

Key options:

| Option | Description |
|--------|-------------|
| `--file, -f` | Path to BTOR2 model (required). |
| `--iteration, -i` | Number of random simulation iterations for sweeping (required). |
| `--bound, -b` | Bounded model checking depth. |
| `--solver_timeout, -s` | Per-query timeout (ms). |
| `--prop_timeout, -p` | Timeout for the final property check (ms). |
| `--dump_input` / `--load_input` | Dump or reuse simulation traces. |
| `--dump_smt` | Emit SMT queries for debugging. |
| `--debug` | Verbose logging. |
| `--find_unsat` | Number of UNSAT proofs needed before switching the sweeper into “apply-only” mode. |
| `--find_sat` | Number of SAT counterexamples needed before switching. |

### 4.2 Sweeper API

Projects embedding FORWORD can invoke the sweeping pass programmatically. Include `sweeper/sweeper_engine.hpp` and drive it as follows:

```cpp
#include "sweeper/sweeper_engine.hpp"

using namespace sweeper;

// Assume `root` is an smt::Term representing the property and
// `solver` is an smt-switch solver already populated with the model.

SweeperOptions options{
    .find_unsat = config.find_unsat,
    .find_sat   = config.find_sat,
    .systems    = { &transition_system }
};

SweeperStats stats = sweeper::sweeper(root, solver, config, options);

if (stats.unsat_count == 0) {
    std::cout << "No UNSAT proofs collected before apply-only mode!" << std::endl;
}

// `root` is rewritten in-place with any discovered equivalences.
// Use `stats` to inspect solver query counts, accumulated runtimes, etc.
```

The API automatically:

1. Collects free symbols and initializes array LUTs (`initialize_arrays`).
2. Runs random/patterned simulation to seed `NodeData`.
3. Executes the `post_order` sweep, falling back to SMT equivalence checks as needed.
4. Updates `root` with any simplifications and returns rich statistics to the caller.

---

## 5. Benchmarks

We collected a set of datapath designs from public GitHub projects (ALU variations, multipliers, AES cores, etc.) and combined functionally equivalent implementations into **miter circuits**. Each miter captures two candidate designs connected in a classic XOR+OR reduction so that a SAT/SMT solver can flag inequivalence as soon as the miter output becomes `1`.

- All miters live under `benchmark/` in **BTOR2** form. You can run `bmc` directly on these files for equivalence checking or counterexample generation.
- If you need alternative formats, use btor2tools and aig2cnf.

This sweeps both candidates, simplifies the miter, and then performs the bounded check. The resulting log includes the sweeping statistics needed for reporting.

---

## 6. Tips & Troubleshooting

- **Dependency mismatch** – re-run the corresponding `contrib/setup-*.sh` script to rebuild local copies.
- **Missing `bitwuzla` pkg-config** – ensure `deps/smt-switch/deps/install/lib/pkgconfig` is on `PKG_CONFIG_PATH` before running `./configure.sh`.
- **Using prebuilt libraries** – package `libwasim.a`, its headers, and third-party `.a` files, then see `scripts/build_with_prebuilt.sh --help` for linking instructions.
- **Python bindings** – configure with `./configure.sh --python` and re-run `make`. The module installs into your active Python site-packages directory.

---

## 7. References

If you use FORWORD in academic work, please cite the corresponding publication (DATE 2026). More examples can be found in `apps/main_cec.cpp` and the `test/` directory.


