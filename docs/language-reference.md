# EdgeLang Language Reference

**EdgeLang** is a domain-specific, statically typed language engineered for embedded, intermittent, and resource-constrained edge computing environments.

---

## 1. Lexical Structure

### 1.1 Keywords
`program`, `int`, `float`, `bool`, `string`, `if`, `else`, `while`, `print`, `checkpoint`, `fault`

### 1.2 Annotations
- `@critical`: Directs the compiler that the annotated variable must be preserved or guaranteed recoverable under all circumstances. Unsafe dropping is rejected.
- `@optional`: Informs the compiler that this variable contains auxiliary or precomputed cache state. It may be checkpointed if the storage budget permits, or discarded when memory is tight.
- `@disposable`: Informs the compiler that this variable is intended as temporary or debug state and must not consume persistent checkpoint memory across crashes.

### 1.3 Built-in Functions
- `read_sensor(channel_id: int) -> int | float`: Simulates reading a physical sensor pin/bus. Marked as an external side-effect: cannot be re-executed spontaneously after a crash without altering hardware semantics or repeating time-sensitive readings.
- `abs(x: int | float) -> int | float`: Absolute value.
- `min(a, b)`: Minimum of two values.
- `max(a, b)`: Maximum of two values.

### 1.4 Comments
- Single-line comments start with `//` and continue to the end of the line.
- Multi-line block comments are enclosed between `/*` and `*/`.

---

## 2. Formal EBNF Grammar

```ebnf
Program         ::= "program" Identifier "{" DeclarationList StatementList "}"

DeclarationList ::= ( VariableDecl )*
StatementList   ::= ( Statement )*

Type            ::= "int" | "float" | "bool" | "string"
Annotation      ::= "@critical" | "@optional" | "@disposable"

VariableDecl    ::= [ Annotation ] Type Identifier [ "=" Expression ] ";"

Statement       ::= BlockStmt
                  | AssignStmt
                  | IfStmt
                  | WhileStmt
                  | PrintStmt
                  | CheckpointStmt
                  | FaultStmt
                  | VariableDecl

BlockStmt       ::= "{" StatementList "}"
AssignStmt      ::= Identifier "=" Expression ";"
IfStmt          ::= "if" "(" Expression ")" Statement [ "else" Statement ]
WhileStmt       ::= "while" "(" Expression ")" Statement
PrintStmt       ::= "print" "(" Expression ")" ";"
CheckpointStmt  ::= "checkpoint" [ "(" [ Identifier ] ")" ] ";"
FaultStmt       ::= "fault" [ "(" StringLiteral ")" ] ";"

Expression      ::= LogicalOrExpr
LogicalOrExpr   ::= LogicalAndExpr ( "||" LogicalAndExpr )*
LogicalAndExpr  ::= EqualityExpr ( "&&" EqualityExpr )*
EqualityExpr    ::= RelationalExpr ( ( "==" | "!=" ) RelationalExpr )*
RelationalExpr  ::= AdditiveExpr ( ( "<" | "<=" | ">" | ">=" ) AdditiveExpr )*
AdditiveExpr    ::= MultiplicativeExpr ( ( "+" | "-" ) MultiplicativeExpr )*
MultiplicativeExpr ::= UnaryExpr ( ( "*" | "/" | "%" ) UnaryExpr )*
UnaryExpr       ::= ( "!" | "-" | "+" ) UnaryExpr | PrimaryExpr
PrimaryExpr     ::= Identifier
                  | IntegerLiteral
                  | FloatLiteral
                  | BooleanLiteral
                  | StringLiteral
                  | FunctionCall
                  | "(" Expression ")"

FunctionCall    ::= Identifier "(" [ ExpressionList ] ")"
ExpressionList  ::= Expression ( "," Expression )*
```

---

## 3. Type System & Semantics

1. **Static Typing**: EdgeLang enforces compile-time type checking.
   - `int`: 32-bit signed integer (4 bytes).
   - `float`: 64-bit IEEE 754 floating point (8 bytes).
   - `bool`: Boolean flag `true` or `false` (1 byte).
   - `string`: String literal used primarily in debug messages and output.
2. **Implicit Conversions**: Automatic widening from `int` to `float` in mixed arithmetic expressions.
3. **Purity & Side Effects**:
   - Arithmetic expressions are referentially pure.
   - `read_sensor()` is impure (external I/O). A variable holding `read_sensor()` cannot be recomputed by re-invoking `read_sensor()` upon reboot, as sensor environments change non-deterministically.
   - `print()` is an observable external effect. Replay of `print()` statements must be avoided during recovery.

---

## 4. Example EdgeLang Programs

### Sensor Calibration Pipeline
```text
program CalibrationPipeline {
    @critical int raw_reading = read_sensor(0);
    int offset = 12;
    int gain = 3;
    
    // Adjusted is purely recomputable from raw_reading, offset, and gain
    int calibrated = (raw_reading - offset) * gain;
    
    @optional int smoothed_cache = calibrated / 2;
    @disposable int debug_loop_count = 0;
    
    checkpoint();
    
    print(calibrated);
}
```
