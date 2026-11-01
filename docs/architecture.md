# Resilience-Aware Compiler Architecture

## Overview

The **Resilience-Aware Compiler for Resource-Constrained Edge Systems** is an optimizing, safety-verifying compiler and execution runtime designed for intermittently powered edge devices (IoT nodes, energy-harvesting sensors, wearable medical monitors, satellites).

Traditional checkpointing mechanisms blindly serialize the entire volatile execution state (registers, stack, live memory), demanding substantial Non-Volatile Memory (NVM/FRAM/Flash) write energy and storage bandwidth. In ultra-constrained devices (often having as little as a few kilobytes of persistent storage and microjoule energy budgets), writing uncompressed full state causes frequent energy depletion and storage exhaustion.

This compiler implements an intelligent, formal **Four-Way State Classification** and **Optimal Recovery Planning** pipeline:
1. **CRITICAL**: Irreducible state that must be written to non-volatile storage to preserve forward progress or semantics (e.g., hardware sensor captures, monotonic counters, decisions influencing external actuators, user `@critical` tags).
2. **RECOMPUTABLE**: State that can be safely, deterministically, and idempotently re-evaluated upon power restoration from already preserved values in strict topological dependency order.
3. **DISPOSABLE**: State that is dead, temporary, or immediately overwritten without being read across the resumption boundary (e.g., loop counters whose lifetime ends, debug counters, intermediate buffers).
4. **OPTIONAL**: Ancillary state (e.g., caches, precomputed rolling averages, user `@optional` tags) that improves post-recovery speed or precision if persistent memory budget permits, but can be safely dropped if the memory budget is constrained.

---

## Complete Pipeline Architecture

```
                       ┌───────────────────────────────┐
                       │     EdgeLang Source Code      │
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │         Lexical Analyzer      │ (Tokens + Line/Col Errors)
                       └──────────────┬────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │       Recursive Parser        │ (Abstract Syntax Tree)
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
                       │ - Value Dependency Graph      │ (DAG of expressions)
                       │ - Recomputability Analysis    │ (Determinism & Side-effect Safety)
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
    │  Recovery Metadata Generator    │ │  Executable Bytecode / VM IR    │
    │  (Machine-readable JSON Plan)   │ │  (Deterministic Edge Runtime)   │
    └────────────────┬────────────────┘ └────────────────┬────────────────┘
                     │                                   │
                     └────────────────┬──────────────────┘
                                      │
                                      ▼
                       ┌───────────────────────────────┐
                       │   Failure-Recovery Simulator  │
                       │ 1. Normal Forward Execution   │
                       │ 2. Checkpoint Serialization   │
                       │ 3. Failure Injection (Power)  │
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

## Module Specifications

### 1. Lexer (`compiler/src/lexer/`)
Tokenizes source streams into strongly typed tokens with precise line and column metadata:
- Keywords: `program`, `int`, `float`, `bool`, `string`, `if`, `else`, `while`, `print`, `checkpoint`, `fault`
- Built-ins: `read_sensor(id)`, `abs(x)`, `min(a,b)`, `max(a,b)`
- Annotations: `@critical`, `@optional`, `@disposable`
- Literals: Integers, floating-point numbers, booleans (`true`/`false`), string literals
- Operators: Arithmetic (`+`, `-`, `*`, `/`, `%`), Relational (`==`, `!=`, `<`, `<=`, `>`, `>=`), Logical (`&&`, `||`, `!`), Assignment (`=`)

### 2. Parser (`compiler/src/parser/`)
Constructs an explicit, typed Abstract Syntax Tree (AST) using recursive-descent parsing with Pratt/precedence climbing for expressions:
- Statement nodes: `VarDeclStmt`, `AssignStmt`, `IfStmt`, `WhileStmt`, `PrintStmt`, `CheckpointStmt`, `FaultStmt`, `BlockStmt`
- Expression nodes: `BinaryExpr`, `UnaryExpr`, `LiteralExpr`, `VariableExpr`, `CallExpr`
- Complete source location retention for diagnostics.

### 3. Semantic Analyzer (`compiler/src/semantic/`)
Validates program semantics and constructs a hierarchical Symbol Table:
- Scope management (program, block, conditional, loop scopes).
- Duplicate variable declaration detection.
- Undeclared identifier detection.
- Strong type checking and type inference validation.
- Validation of annotations (e.g. verifying `@critical` is not placed on conflicting or dead constructs).

### 4. Control-Flow Graph (`compiler/src/cfg/`)
Translates AST statements into a directed graph of basic blocks:
- Basic blocks contain linear sequences of three-address / statement representations.
- Explicit Entry block, Exit block, and Edge types (Normal, TrueBranch, FalseBranch, LoopBack, LoopExit).
- Full JSON serialization for visual UI rendering and program slicing.

### 5. Program Analysis Engine (`compiler/src/analysis/`)
- **Liveness Analysis**: Computes `USE[B]`, `DEF[B]`, `LIVE_IN[B]`, and `LIVE_OUT[B]` sets using backward data-flow fixed-point iteration:
  $$LIVE\_IN[B] = USE[B] \cup (LIVE\_OUT[B] \setminus DEF[B])$$
  $$LIVE\_OUT[B] = \bigcup_{S \in succ(B)} LIVE\_IN[S]$$
- **Def-Use & Use-Def Chains**: Direct references connecting definitions of variables to all possible consumption points.
- **Value Dependency Graph**: Directed Acyclic Graph (DAG) representing the data dependencies of every variable calculation.
- **Recomputability Oracle**: Evaluates if a variable's value can be regenerated purely through side-effect-free, deterministic arithmetic from an available saved frontier. Hardware sensor reads (`read_sensor`) and volatile external mutations are flagged non-recomputable.

### 6. Four-Way State Classifier (`compiler/src/classifier/`)
A deterministic, rule-based decision engine that categorizes every variable at each program checkpoint into:
- `CRITICAL`
- `RECOMPUTABLE`
- `DISPOSABLE`
- `OPTIONAL`
Includes a human-readable justification log for compiler transparency.

### 7. Recovery Planner (`compiler/src/planner/`)
Solves the minimal checkpoint storage problem subject to:
1. Soundness: Every live variable needed post-recovery is either saved or guaranteed safely reconstructible.
2. Topological Consistency: Recomputation sequence respects all dependencies.
3. Budget Respect: If user specifies maximum NVM budget (e.g. 16 bytes), optional variables are dropped and optimal trade-offs are applied.
4. Feasibility: Explicitly warns if critical state exceeds the physical budget.

### 8. Intermediate Representation & Bytecode VM (`compiler/src/ir/`, `compiler/src/runtime/`)
Executes EdgeLang bytecode in a deterministic virtual machine:
- Simulates hardware registers, volatile memory, and non-volatile checkpoint buffer.
- Real-time serialization and deserialization of checkpoint payloads.
- Injectable transient power-loss failures at specific instructions, basic blocks, or cycles.
- Resumes execution after failure and runs the correctness oracle against a failure-free reference trace.

### 9. REST API & Modern Web Dashboard (`api/`, `frontend/`)
Provides a responsive, full-featured systems-engineering web UI to visualize the AST, inspect CFG nodes, adjust memory budgets, step through simulated failures, and compare all 4 checkpoint strategies.
