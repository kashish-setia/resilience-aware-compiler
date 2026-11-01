# Failure and Recovery Model

This document specifies the operational semantics, intermittent execution lifecycle, failure injection protocols, and correctness verification oracle.

---

## 1. System & Failure Model

Edge systems operate in intermittent execution environments powered by energy harvesting (solar, RF, vibrational) or unstable batteries:
- **Volatile Execution State**: CPU registers, program counter, and SRAM are completely cleared whenever power is interrupted or a software crash occurs.
- **Non-Volatile Storage (NVM)**: Battery-backed SRAM, FRAM, or Flash retains state across power losses. Writes to NVM are energy-expensive, slow, and wear down endurance.
- **Intermittent Power Cycles**: Execution proceeds until energy drops below operating threshold $V_{min}$. A power failure interrupts execution at an arbitrary statement or instruction.
- **Atomic Checkpoints**: Checkpoints write a consistent snapshot of the selected variables to NVM. A two-phase commit or transactional header ensures that partially written checkpoints are detected as invalid.

---

## 2. Checkpoint Serialization & Layout

A checkpoint record in NVM consists of:
1. `header`:
   - `magic`: 32-bit validation identifier (`0xED9EC0DE`).
   - `checkpoint_id`: Incremental sequence ID.
   - `program_counter`: Instruction pointer / Statement index to resume from.
   - `timestamp_cycle`: Virtual cycle stamp.
   - `variable_count`: Number of serialized variables.
2. `payload`:
   - For each saved variable:
     - `name_hash` / `symbol_id`: 16-bit identifier.
     - `type_tag`: 8-bit (`INT`, `FLOAT`, `BOOL`, `STRING`).
     - `size_bytes`: 8-bit length.
     - `raw_data`: 4/8 bytes payload.
3. `checksum`: CRC32 validation across the header and payload.

---

## 3. Recovery Protocol

Upon power resumption or post-crash reboot:
1. **Validation**: Read NVM headers, verify CRC32, identify latest intact checkpoint $C_k$.
2. **State Restoration**: Re-populate volatile memory with saved variables from $C_k$.
3. **Recomputation Phase**:
   - The recovery metadata provides a **Topological Recomputation Schedule**:
     $$R = [v_{r1}, v_{r2}, \dots, v_{rn}]$$
   - For each $v_{rj} \in R$:
     Evaluate its expression using the restored inputs. Write result into volatile memory.
4. **Resumption**:
   - Set execution pointer to the resumption point associated with $C_k$.
   - Re-enable volatile execution.

---

## 4. Correctness Oracle

To verify that recovery preserves program correctness without semantic divergence:
1. **Reference Run**: Execute the program end-to-end under a failure-free environment with deterministic mock inputs. Record the sequence of all observable outputs ($\Omega_{ref}$) and terminal variable states ($\Sigma_{ref}$).
2. **Fault Injection Run**:
   - Execute until target checkpoint $C_k$.
   - Serialize state using the evaluated strategy.
   - Simulate sudden failure at injection point $F$ (statement or cycle).
   - Clear volatile state completely.
   - Execute recovery protocol (restore + recompute).
   - Resume execution to completion. Record recovered outputs ($\Omega_{rec}$) and final state ($\Sigma_{rec}$).
3. **Equivalence Invariant**:
   $$\Omega_{rec} \equiv \Omega_{ref} \quad \text{and} \quad \Sigma_{rec} \equiv \Sigma_{ref}$$
   Any deviation in output sequence, values, or terminal live variables signals a recovery defect.
