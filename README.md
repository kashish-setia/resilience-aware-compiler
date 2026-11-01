# Resilience-Aware Compiler for Resource-Constrained Edge Systems

An optimizing, safety-verifying compiler and execution runtime engineered for ultra-resource-constrained, intermittently powered edge devices (IoT nodes, energy-harvesting sensors, wearable medical monitors, cubesats).

Traditional checkpointing mechanisms blindly serialize the entire volatile execution state (CPU registers, stack, live memory) to Non-Volatile Memory (NVM / FRAM / Flash), consuming prohibitive amounts of energy and storage bandwidth. This compiler performs rigorous program analysis—including Control-Flow Graph (CFG) construction, backward fixed-point Liveness analysis, Def-Use chains, and Value Dependency DAG analysis—to partition state into a formal **Four-Way Classification** and generate an optimal, budget-constrained recovery plan:

1. **CRITICAL**: Irreducible state preserved in persistent storage (hardware sensor captures, monotonic counters, decisions influencing external actuators, user `@critical` tags).
2. **RECOMPUTABLE**: State reconstructed safely, deterministically, and idempotently upon recovery from available saved inputs in topological dependency order.
3. **DISPOSABLE**: State that is dead, temporary, or immediately overwritten without being read across recovery (dead variables, debug counters).
4. **OPTIONAL**: Ancillary state (caches, precomputed rolling averages, user `@optional` tags) saved when the persistent memory budget allows, but safely dropped if storage is constrained.

---

## 1. System Architecture

```
                       ┌───────────────────────────────┐
                       │     EdgeLang Source Code      │
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │   Lexer & Recursive Parser    │ (Tokens + Typed AST)
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │       Semantic Analyzer       │ (Symbol Table + Type Checking)
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │   Control-Flow Graph (CFG)    │ (Basic Blocks + Branch/Loop Edges)
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │     Program Analysis Suite    │
                       │ - Backward Liveness Analysis  │ (USE, DEF, LIVE_IN, LIVE_OUT)
                       │ - Def-Use & Use-Def Chains    │
                       │ - Value Dependency DAG        │ (Expression Graph)
                       │ - Recomputability Oracle      │ (Purity, Determinism & Cycles)
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │   Four-Way State Classifier   │ (Transparent, Deterministic Rule Engine)
                       │   CRITICAL / RECOMPUTABLE /   │ (Explanations & Storage Costs)
                       │   DISPOSABLE / OPTIONAL       │
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │       Recovery Planner        │ (Budget-Constrained Optimization)
                       │ - Checkpoint Set Selection    │ (Topological Recomputation Ordering)
                       │ - Budget Enforcer & Feasibility│
                       └──────────────┬────────────────┘
                                      │
                     ┌────────────────┴────────────────┐
                     │                                 │
                     ▼                                 ▼
    ┌─────────────────────────────────┐ ┌─────────────────────────────────┐
    │  Recovery Metadata Generator    │ │    Deterministic Bytecode VM    │
    │  (Machine-readable JSON Plan)   │ │  (Simulated NVM + Fault Inject) │
    └────────────────┬────────────────┘ └────────────────┬────────────────┘
                     │                                   │
                     └────────────────┬──────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │   Failure-Recovery Simulator  │
                       │ 1. Forward Execution          │
                       │ 2. Checkpoint Serialization   │
                       │ 3. Transient Power Crash      │
                       │ 4. State Restoration from NVM │
                       │ 5. Dependency Recomputation   │
                       │ 6. Resumed Execution          │
                       │ 7. Correctness Verification   │ (Oracle vs Failure-Free Run)
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │  Benchmark & Reporting Suite  │
                       │ - Checkpoint Everything       │
                       │ - Liveness-Only               │
                       │ - Manual-Annotation           │
                       │ - Resilience-Aware            │
                       └───────────────────────────────┘
```

---

## 2. Key Features

- **End-to-End Compiler Pipeline**: Hand-written Lexer, Recursive-Descent Parser, AST, Semantic Analyzer, Symbol Table, CFG Generator, Data-Flow Analyzer, 4-Way Classifier, Recovery Planner, Bytecode Generator, and Deterministic Virtual Machine.
- **Formally Verified Backward Liveness**: Iterative data-flow fixed-point solver computing `USE`, `DEF`, `LIVE_IN`, and `LIVE_OUT` facts for every basic block.
- **Safe Recomputability Engine**: Traverses the Value Dependency DAG to prove pure mathematical determinism and topological reconstructibility. Automatically flags non-deterministic hardware inputs (`read_sensor`) as non-recomputable.
- **Budget-Aware Optimization**: Permits user-configured physical NVM budgets (e.g., 16 bytes). Automatically drops `@optional` state when tight and alerts if critical state exceeds physical limits.
- **Interactive Failure Simulator & Oracle**: Injects transient crashes at arbitrary execution points (cycles or statement IDs), clears volatile RAM, restores state from NVM, recomputes values in topological order, and verifies that observable outputs match reference failure-free execution.
- **Built-in Embedded HTTP API & Dashboard**: Local REST API embedded directly in C++ (listening on port 8080) powering a React 19 + TypeScript + Vite systems engineering web interface.
- **Empirical 4-Strategy Benchmark Comparison**: Runs across 9 benchmark programs comparing *Checkpoint Everything*, *Liveness-Only*, *Manual-Annotation*, and *Resilience-Aware*. Demonstrates up to **87.5% memory reduction** while maintaining **100% recovery correctness**.

---

## 3. Technology Stack

- **Core Compiler & Simulator**: C++17 (`gcc`, `g++`, `clang`, MSVC compatible).
- **Build System**: CMake (>= 3.20) + Ninja / MinGW.
- **Automated Test Harness**: CTest + comprehensive C++ test runner.
- **Embedded REST API**: C++ sockets (`winsock2` on Windows, BSD sockets on POSIX).
- **Web Frontend Dashboard**: React 19, TypeScript, Vite, Tailwind-style design system.
- **Data Exchange**: JSON format throughout all stages.

---

## 4. EdgeLang Syntax & DSL Specification

```text
program TemperatureMonitor {
    @critical int raw = read_sensor(0);
    int calibration = 3;
    int adjusted = raw + calibration;
    int filtered = adjusted * 2;
    int threshold = 50;
    @optional int cached_status = 1;
    @disposable int debug_tick = 99;
    
    checkpoint();
    
    bool alert = filtered > threshold;
    print(alert);
}
```

### Keywords and Types
- Types: `int` (4 bytes), `float` (8 bytes), `bool` (1 byte), `string`.
- Annotations: `@critical`, `@optional`, `@disposable`.
- Statements: `if (cond) { ... } else { ... }`, `while (cond) { ... }`, assignments, `print(expr);`.
- Edge Primitives:
  - `read_sensor(channel_id)`: Hardware input (side-effect: non-recomputable).
  - `checkpoint()`: Snapshot barrier.
  - `fault("reason")`: Simulated power loss trigger.

---

## 5. Prerequisites

- **C++ Compiler**: GCC / G++ (>= 11), Clang (>= 14), or MSVC 2019+.
- **CMake**: Version >= 3.20.
- **Build Tool**: Ninja or MinGW32-Make.
- **Node.js**: Version >= 18 (for the web frontend).

---

## 6. Build Instructions

### Building the C++ Compiler & Test Suite

```powershell
# Navigate to project root
cd resilience-aware-compiler

# Configure with CMake and Ninja
cmake -B build -G Ninja

# Compile the compiler library, CLI (edgec), and test suite
cmake --build build
```

This generates:
- `build/libedge_compiler_lib.a` (Core compiler static library)
- `build/edgec.exe` (Command-line interface)
- `build/edge_test_suite.exe` (Automated verification test runner)

---

## 7. Running the CLI (`edgec`)

```powershell
# 1. Compile a program and display disassembly & recovery plan
.\build\edgec.exe examples\temperature_logger.edge

# 2. Output complete compiler pipeline JSON artifact
.\build\edgec.exe --json examples\temperature_logger.edge

# 3. Inspect Abstract Syntax Tree (AST)
.\build\edgec.exe --ast examples\temperature_logger.edge

# 4. Inspect Control-Flow Graph (CFG) basic blocks and edges
.\build\edgec.exe --cfg examples\temperature_logger.edge

# 5. Display backward liveness data-flow facts (USE, DEF, LIVE_IN, LIVE_OUT)
.\build\edgec.exe --liveness examples\temperature_logger.edge

# 6. View 4-way variable classifications and decision rationale
.\build\edgec.exe --classify examples\temperature_logger.edge

# 7. Generate a recovery plan with a 16-byte memory budget constraint
.\build\edgec.exe --plan examples\temperature_logger.edge --budget 16

# 8. Simulate failure injection, NVM restore, and recomputation verification
.\build\edgec.exe --simulate examples\temperature_logger.edge --strategy RESILIENCE_AWARE

# 9. Run the 4-strategy benchmark comparison suite
.\build\edgec.exe --benchmark

# 10. List all built-in benchmark programs
.\build\edgec.exe --examples

# 11. Launch the local embedded REST API server
.\build\edgec.exe --serve --port 8080
```

---

## 8. Running the Automated Test Suite

```powershell
# Run the test suite directly
.\build\edge_test_suite.exe

# Or run via CTest
ctest --test-dir build --output-on-failure
```

The test harness executes 15 comprehensive unit and end-to-end integration tests:
- Lexer token scanning & lexical errors
- Recursive-descent AST construction & operator precedence
- Semantic analyzer: undeclared variables, duplicate variables, type checking
- Control-Flow Graph & backward liveness fixed-point convergence
- Def-Use and Value Dependency DAG verification
- Four-way state classification accuracy
- Recovery planner feasibility under budget constraints
- Power failure injection, NVM serialization, restoration, and recomputation
- Correctness oracle validation against reference execution
- Comparative validation of all four benchmark strategies
- Validation across all 9 built-in edge programs

---

## 9. Launching the Interactive Web Dashboard

```powershell
# Terminal 1: Launch the C++ REST API Server
.\build\edgec.exe --serve --port 8080

# Terminal 2: Launch the React Dashboard
cd frontend
npm run dev
```

Open your browser at `http://localhost:3000` to access the interface:
- **Overview Dashboard**: Key performance metrics, storage savings, live backend health status.
- **Source Editor**: In-browser EdgeLang editor with example selector and instant diagnostics.
- **Pipeline Visualizer**: Step through Lexer -> Parser -> Semantic -> CFG -> Liveness -> Classifier -> Planner -> CodeGen.
- **Variable Classification**: Filterable table of CRITICAL, RECOMPUTABLE, OPTIONAL, and DISPOSABLE variables with decision rationales.
- **CFG Explorer**: Interactive basic block viewer inspecting USE, DEF, LIVE_IN, and LIVE_OUT facts.
- **Recovery Planner**: Dynamic memory budget slider evaluating feasibility and byte savings in real-time.
- **Failure Simulator**: Step-through event timeline of crash, NVM restoration, and dependency recomputation.
- **Benchmark Matrix**: Side-by-side comparison tables across all 4 strategies with CSV export.

---

## 10. Benchmark Comparison Summary

Measured across 9 benchmark programs using the automated compiler engine:

| Benchmark Program | Checkpoint Everything | Liveness-Only | Manual-Annotation | Resilience-Aware | Storage Reduction | Recovery Correctness |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **Temperature Monitor** | 29 B | 16 B | 4 B (PASSED) | **8 B** | **-72.4%** | **100% PASSED** |
| **Sensor Calibration** | 28 B | 12 B | 4 B (PASSED) | **8 B** | **-71.4%** | **100% PASSED** |
| **Heart-Rate Monitor** | 22 B | 13 B | 8 B (PASSED) | **12 B** | **-45.5%** | **100% PASSED** |
| **Arithmetic Chain** | 32 B | 8 B | 4 B (PASSED) | **4 B** | **-87.5%** | **100% PASSED** |
| **Environmental Control** | 16 B | 12 B | 8 B (PASSED) | **12 B** | **-25.0%** | **100% PASSED** |
| **Loop Accumulator** | 16 B | 8 B | 8 B (PASSED) | **8 B** | **-50.0%** | **100% PASSED** |
| **Telemetry Logger** | 20 B | 8 B | 4 B (PASSED) | **4 B** | **-80.0%** | **100% PASSED** |
| **Polynomial Pipeline** | 24 B | 8 B | 4 B (PASSED) | **4 B** | **-83.3%** | **100% PASSED** |
| **Multi-Sensor Fusion** | 16 B | 16 B | 8 B (PASSED) | **12 B** | **-25.0%** | **100% PASSED** |

> **Key Takeaway**: With dependency recomputation and safety preservation integrated into the recovery planner and simulator, the Resilience-Aware Compiler achieves up to **87.5% memory reduction** while **guaranteeing 100% recovery correctness** across all benchmark programs.

---

## 11. Known Limitations & Future LLVM Extension

- **Current Limitations**: The present version targets a dedicated, deterministic Bytecode Virtual Machine with simulated Non-Volatile Memory. Dynamic heap allocations and pointers are intentionally restricted to preserve embedded memory safety.
- **Future LLVM Extension**: An LLVM Middle-End Pass can ingest LLVM IR (`opt`), perform the 4-way classification over LLVM Static Single Assignment (SSA) virtual registers, insert LLVM intrinsic `@llvm.edge.checkpoint()` calls, and emit optimized target assembly (ARM Cortex-M0+/M4, RISC-V RV32EMC) using `llc`.
