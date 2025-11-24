# FORWORD (DATE 2026)

FORWORD accelerates datapath equivalence checking via word-level sweeping built on symbolic simulation and SMT validation. This README focuses on the four essentials: setup & build, benchmark corpus, sweeper API, and citation.

---

## 1. Setup & Build

```bash
sudo apt update && sudo apt install -y \
    build-essential cmake default-jre libgmp-dev \
    libboost-all-dev libgflags-dev flex bison git
pip3 install --user toml

./contrib/setup-glog.sh
./contrib/setup-bison.sh
./contrib/setup-btor2tools.sh
./contrib/setup-smt-switch.sh

./configure.sh [--static-lib]
cd build && make -j$(nproc)
```

---

## 2. Benchmark Suite

- `benchmark/` hosts BTOR2 miters built from GitHub datapath implementations (ALUs, multipliers, AES, etc.). Each `.btor2` file contains two designs wired into XOR/OR detection logic, ready for equivalence checking.
- Run a case:

  ```bash
  ./build/bmc --file benchmark/mul/mul_1.btor2 \
      --iteration 100 --bound 10 --find_unsat 20 --find_sat 1
  ```

---

## 3. Sweeper API

```cpp
#include "sweeper/sweeper_engine.hpp"

sweeper::SweeperOptions opts{
    .find_unsat = config.find_unsat,
    .find_sat   = config.find_sat,
    .systems    = { &ts }
};

sweeper::SweeperStats stats = sweeper::sweeper(root, solver, config, opts);
```

- `root` (smt::Term) is updated in place after sweeping.
- `stats` reports SMT query counts, UNSAT/SAT proofs, timing, and map sizes.
- The API internally performs free-symbol discovery, array LUT initialization, randomized simulation, and `post_order` sweeping. Callers only supply the transition system pointer(s) and stopping thresholds.

---

## 4. Citation

To be updated after DATE 2026.


