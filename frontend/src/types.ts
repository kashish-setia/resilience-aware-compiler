export type StateClass = 'CRITICAL' | 'RECOMPUTABLE' | 'DISPOSABLE' | 'OPTIONAL';

export type CheckpointStrategy = 
  | 'CHECKPOINT_EVERYTHING'
  | 'LIVENESS_ONLY'
  | 'MANUAL_ANNOTATION'
  | 'RESILIENCE_AWARE';

export interface Diagnostic {
  level: 'info' | 'warning' | 'error';
  message: string;
  line: number;
  column: number;
}

export interface Token {
  kind: string;
  lexeme: string;
  line: number;
  column: number;
}

export interface SymbolInfo {
  name: string;
  type: string;
  annotation: string;
  sizeBytes: number;
  statementId: number;
  line: number;
  column: number;
  isExternalSensor: boolean;
}

export interface CFGEdge {
  target: number;
  type: string;
}

export interface BasicBlock {
  id: number;
  label: string;
  hasCheckpoint: boolean;
  statements: any[];
  predecessors: number[];
  successors: CFGEdge[];
  use: string[];
  def: string[];
  liveIn: string[];
  liveOut: string[];
}

export interface CFGData {
  entryBlockId: number;
  exitBlockId: number;
  blocks: BasicBlock[];
}

export interface LivenessFacts {
  iterations: number;
  blockFacts: {
    blockId: number;
    use: string[];
    def: string[];
    liveIn: string[];
    liveOut: string[];
  }[];
}

export interface VariableClassification {
  name: string;
  type: string;
  classification: StateClass;
  reason: string;
  isSelectedForCheckpoint: boolean;
  isRecomputable: boolean;
  storageSizeBytes: number;
  recomputeCost: number;
  annotation: string;
  dependencies: string[];
}

export interface CheckpointPlan {
  checkpointId: number;
  strategy: CheckpointStrategy;
  memoryBudgetBytes: number;
  isFeasible: boolean;
  infeasibilityReason: string;
  totalSavedBytes: number;
  baselineEverythingBytes: number;
  bytesSaved: number;
  savingsPercent: number;
  savedVariables: string[];
  recomputedVariables: string[];
  recomputeOrder: string[];
  discardedVariables: string[];
  optionalVariablesExcluded: string[];
  decisionLog: string[];
}

export interface SimulationEvent {
  phase: string;
  cycle: number;
  message: string;
}

export interface SimulationResult {
  strategy: CheckpointStrategy;
  isCorrect: boolean;
  failurePointDescription: string;
  checkpointSizeBytes: number;
  baselineSizeBytes: number;
  savedVarCount: number;
  recomputedVarCount: number;
  recoveryTimeMs: number;
  totalCycles: number;
  referenceOutputs: string[];
  recoveredOutputs: string[];
  events: SimulationEvent[];
}

export interface StrategyComparison {
  strategyName: string;
  storageBytes: number;
  recoveryTimeMs: number;
  savedVarCount: number;
  recomputedVarCount: number;
  isCorrect: boolean;
  savingsPercent: number;
  codeSizeInstructions: number;
}

export interface BenchmarkSuiteResult {
  benchmarkName: string;
  description: string;
  compilationTimeMs: number;
  comparisons: StrategyComparison[];
}

export interface CompilationResult {
  success: boolean;
  programName: string;
  compilationTimeMs: number;
  diagnostics: Diagnostic[];
  tokens: Token[];
  ast: any;
  symbolTable: SymbolInfo[];
  cfg: CFGData | null;
  liveness: LivenessFacts | null;
  defUse: { variables: { name: string; defs: number[]; uses: number[] }[] } | null;
  dependencyGraph: { nodes: any[] } | null;
  classifications: VariableClassification[];
  recoveryPlan: CheckpointPlan | null;
  bytecode: any;
  disassembled: string;
}

export interface ExampleProgram {
  id: string;
  title: string;
  description: string;
  category: string;
  sourceCode: string;
}
