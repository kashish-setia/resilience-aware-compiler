# Program Analysis Methodology

This document outlines the theoretical formulations and implementation mechanics behind the data-flow, dependency, and recomputability analyses implemented in the compiler.

---

## 1. Control-Flow Graph (CFG) Construction

The CFG represents a program as a directed graph $G = (V, E)$, where:
- $V$ is the set of Basic Blocks $B_i$, each containing a straight-line sequence of statements with single entry and single exit.
- $E$ is the set of directed edges $(B_i, B_j)$ representing permissible transfers of control.
- Special nodes $B_{entry}$ and $B_{exit}$ delimit program scope.

### Edge Types
- `FALLTHROUGH`: Unconditional sequence between consecutive linear blocks.
- `TRUE_BRANCH` / `FALSE_BRANCH`: Conditional edges emerging from `if` conditions or loop headers.
- `LOOP_BACK`: Feedback edge from the end of a `while` loop body to the evaluation header.
- `LOOP_EXIT`: Exit edge from a `while` header to the post-loop continuation block.

---

## 2. Backward Liveness Analysis

Liveness analysis determines which variables hold values at a given program point that may be read before their next reassignment.

### Formulations
For each basic block $B$:
- $\text{DEF}[B]$: Variables defined (assigned) in $B$ prior to any use within $B$.
- $\text{USE}[B]$: Variables read in $B$ before any definition within $B$.
- $\text{LIVE\_OUT}[B] = \bigcup_{S \in \text{succ}(B)} \text{LIVE\_IN}[S]$
- $\text{LIVE\_IN}[B] = \text{USE}[B] \cup (\text{LIVE\_OUT}[B] \setminus \text{DEF}[B])$

### Fixed-Point Computation
The compiler initializes $\text{LIVE\_IN}[B] = \emptyset$ and $\text{LIVE\_OUT}[B] = \emptyset$ for all $B$. It iteratively updates the equations in reverse topological order (or worklist order) until:
$$\Delta(\text{LIVE\_IN}, \text{LIVE\_OUT}) = 0$$
Because the domain is a finite powerset lattice and transfer functions are monotonic, convergence is guaranteed by the Knaster-Tarski theorem.

---

## 3. Def-Use & Dependency Analysis

For every variable $v \in V$:
- **Def-Use Chains**: Maps each definition statement $D(v)$ to the set of statement IDs $\{U_1(v), U_2(v), \dots\}$ that read $v$.
- **Use-Def Chains**: Maps each usage of $v$ to the reaching definition(s) of $v$.
- **Data Dependency Graph (DDG)**: A directed graph $D = (N, E_d)$ where each node $n \in N$ represents a variable definition, and an edge $(n_1, n_2) \in E_d$ exists if the computation of $n_2$ directly depends on the value of $n_1$.

---

## 4. Recomputability Analysis

A variable $v$ is classified as **RECOMPUTABLE** at checkpoint $C$ if and only if all of the following conditions hold:

1. **Deterministic Purity**: The definition of $v$ is a pure mathematical function of its inputs:
   $$\text{Def}(v) = f(i_1, i_2, \dots, i_k)$$
   where $f$ contains no external I/O (e.g., `read_sensor`), no random functions, and no unmodeled volatile hardware registers.
2. **Availability of Predecessors**: Every direct input $i_j$ is either:
   - Part of the checkpoint set $S_{saved}$, or
   - Itself recursively recomputable from $S_{saved}$.
3. **No Intervening Mutation / Side Effect**: The definitions of $i_j$ must not have been clobbered by subsequent assignments in the path between $D(v)$ and $C$ unless versioned / SSA-tracked.
4. **Energy / Storage Amortization**:
   $$\text{Cost}_{recompute}(v) \le \text{Threshold} \times \text{Cost}_{checkpoint\_write}(v)$$
   On edge devices, writing to Flash/FRAM consumes significant nanojoules per bit compared to simple arithmetic ALU operations. Recomputation is vastly preferred whenever inputs are already saved.

---

## 5. Four-Way State Classification Rules

| Class | Definition & Decision Rule | Storage Action |
| :--- | :--- | :--- |
| **CRITICAL** | Annotated `@critical`, OR hardware sensor readings, OR non-recomputable variables live across checkpoint whose loss causes semantic failure. | Must be saved to NVM. |
| **RECOMPUTABLE** | Variable is live across checkpoint, not annotated `@critical`, and can be safely recomputed from saved or critical inputs via pure arithmetic in topological order. | Excluded from NVM write; reconstructed at recovery. |
| **DISPOSABLE** | Variable is dead at checkpoint ($\notin \text{LIVE\_OUT}$), OR annotated `@disposable`, OR debug temporary that has no future influence. | Discarded; no NVM write, no recompute. |
| **OPTIONAL** | Annotated `@optional`, OR auxiliary cache/metrics. Saved only if physical memory budget allows after critical items are accommodated. | Conditionally saved if $\text{Budget} \ge \text{Cost}$; otherwise safely omitted. |
