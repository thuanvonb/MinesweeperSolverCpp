# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

**Native (MSVC):** Open `MinesweeperSolver.sln` in Visual Studio and build, or use MSBuild.

**Native (g++/clang++):**
```bash
g++ -std=c++17 -O2 Board.cpp Cell.cpp EndgameSolver.cpp Group.cpp MinesweeperSolver.cpp Solver.cpp Utils.cpp -o MinesweeperSolver.exe
```

**WebAssembly (Emscripten):**
```bash
em++ -std=c++17 -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS="[\"ccall\",\"cwrap\",\"getValue\",\"setValue\",\"HEAP32\"]" -s MODULARIZE=1 -s EXPORT_NAME="MinesweeperModule" -s EXPORTED_FUNCTIONS="[\"_solveBoard\",\"_malloc\",\"_free\"]" -s ASYNCIFY=1 Board.cpp Cell.cpp EndgameSolver.cpp Group.cpp MinesweeperSolver.cpp Solver.cpp Utils.cpp -o docs/MinesweeperSolver.js
```

**Run (native):** reads from `minesweeper.inp` (format: `height width mines` then grid values). Only runs when `BUILD_EMSDK` is not defined in `MinesweeperSolver.cpp`.

## Architecture

The solver computes per-cell mine probabilities for a Minesweeper board. There are two build targets: a native CLI that reads from `minesweeper.inp`, and a WASM module (via `BUILD_EMSDK` define) exposing `solveBoard()` as a C extern for the web frontend in `docs/`.

### Core Classes

- **Cell** (`Cell.h/cpp`): Represents a board cell. `value` encodes state: `-4` floating, `-3` safe, `-2` flag, `-1` undiscovered, `0-8` number. Tracks `minePerc` (mine probability) and `valuePerc[9]` (number probabilities). Each cell holds pointers to its containing `Group`s.

- **Board** (`Board.h/cpp`): 2D grid of `Cell`s. Constructed from `vector<vector<int>>` raw data. Tracks `unsolved` cell count.

- **Group** (`Group.h/cpp`): A constraint: a set of cells that must contain between `minV` and `maxV` mines. Created from numbered cells (value = neighbor mine count minus already-flagged neighbors). Groups can be crossed (split on overlap), synced (tighten bounds), and merged. The `cross()` operation between overlapping groups produces refined sub-groups with tighter constraints.

- **Solver** (`Solver.h/cpp`): Main solving engine with two phases:
  1. **Deterministic** (`iterativeSolve`): Repeatedly crosses groups, syncs constraints, and applies forced deductions (all-safe or all-mine groups) until no progress.
  2. **Probabilistic** (`generalSolve`): Partitions remaining groups into independent chains (connected components via BFS), enumerates all valid configurations per chain (`solveRec`), then combines chain solutions with Bayesian weighting over remaining unassigned mines using binomial coefficients.

- **Solver::tryWarp**: Given a cell and whether it's a mine, creates a "warped" board with that assumption, re-solves, and samples a consistent mine configuration using backward DP over chain mine counts and random sampling.

### Key Constants (`Macros.h`)

Cell values: `CELL_FLOATING(-4)`, `CELL_SAFE(-3)`, `CELL_FLAG(-2)`, `CELL_UNDISCOVERED(-1)`. Group relations: `RELATION_SUBSET`, `RELATION_EQUAL`, `RELATION_SUPERSET`, `RELATION_DISJOINT`, `RELATION_JOINT`.

### Solver Algorithm Flow

`generalSolve(mines)` → `iterativeSolve()` (deterministic loop: `crossAllGroups` → `syncAllGroups` → `apply` → `filter`) → `getGroupChains()` (BFS partitioning) → `solveChain()` per chain (recursive enumeration with pruning) → `combineChainMineCount()` (Cartesian product across chains) → Bayesian probability computation using `computeNormalizedBinomials()`.

### Web Frontend

`docs/` contains the web UI (`index.html`, `Minesweeper.js`, `style.css`) that loads `MinesweeperSolver.wasm` via the Emscripten-generated JS glue. The `solveBoard(nrows, ncols, nums, mines, prob)` function is the WASM entry point. `docs/minewarp/` contains the mine warp feature UI.

## Input Format (`minesweeper.inp`)

```
height width mines
<grid values: -3=safe, -2=flag, -1=undiscovered, 0-8=number>
```

## Platform Notes

- `getCombinations()` uses `__popcnt` on MSVC and `__builtin_popcount` on GCC/Clang (compile-time conditional).
- `EndgameSolver` is a stub class wrapping `Solver` — endgame solving is currently in development on the `dev-endgame-solver` branch.
