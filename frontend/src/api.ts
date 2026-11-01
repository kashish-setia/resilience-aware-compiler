import { CompilationResult, SimulationResult, BenchmarkSuiteResult, ExampleProgram, CheckpointStrategy } from './types';

const API_BASE = '/api';

export async function checkServerHealth(): Promise<boolean> {
  try {
    const res = await fetch(`${API_BASE}/health`, { method: 'GET', signal: AbortSignal.timeout(1500) });
    return res.ok;
  } catch {
    return false;
  }
}

export async function fetchExamples(): Promise<ExampleProgram[]> {
  try {
    const res = await fetch(`${API_BASE}/examples`, { signal: AbortSignal.timeout(2000) });
    if (res.ok) {
      return await res.json();
    }
  } catch {}

  // Fallback default examples if backend server is not running
  return [
    {
      id: 'temperature_logger',
      title: 'Temperature Monitor',
      description: 'Continuous temperature logger reading sensor, applying calibration, scaling, and threshold alert.',
      category: 'Edge Sensor',
      sourceCode: `program TemperatureMonitor {
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
}`
    },
    {
      id: 'sensor_calibration',
      title: 'Sensor Calibration Pipeline',
      description: 'Industrial sensor zero-offset calibration, linear gain multiplication, and range normalization.',
      category: 'Calibration',
      sourceCode: `program SensorCalibration {
    @critical int raw_in = read_sensor(0);
    int zero_offset = 10;
    int gain_multiplier = 4;
    int calibrated = (raw_in - zero_offset) * gain_multiplier;
    @optional int cached_average = calibrated / 2;
    @disposable int debug_seq = 1001;
    checkpoint();
    int normalized = calibrated + 5;
    print(normalized);
}`
    },
    {
      id: 'heart_rate_monitor',
      title: 'Heart-Rate Threshold Monitor',
      description: 'Medical monitoring node checking for bradycardia and tachycardia conditions with mock telemetry.',
      category: 'Healthcare',
      sourceCode: `program HeartRateMonitor {
    @critical int pulse_raw = read_sensor(1);
    int baseline = 60;
    int delta = pulse_raw - baseline;
    int bpm = baseline + delta;
    bool tachycardia = bpm > 100;
    bool bradycardia = bpm < 50;
    @optional int cache_zone = 2;
    checkpoint();
    print(bpm);
    print(tachycardia);
}`
    },
    {
      id: 'arithmetic_chain',
      title: 'Arithmetic Dependency Chain',
      description: 'Deep multi-step arithmetic transformation demonstrating dependency reconstruction ordering.',
      category: 'Compute-Heavy',
      sourceCode: `program ArithmeticChain {
    @critical int start_seed = read_sensor(0);
    int step1 = start_seed + 10;
    int step2 = step1 * 3;
    int step3 = step2 - 15;
    int step4 = step3 / 2;
    int step5 = step4 + 7;
    @disposable int temp_calc = 888;
    checkpoint();
    int final_result = step5 * 2;
    print(final_result);
}`
    },
    {
      id: 'conditional_branches',
      title: 'Conditional Environmental Control',
      description: 'Multi-path branching control adjusting actuation states according to environmental temperature.',
      category: 'Control Logic',
      sourceCode: `program EnvironmentalControl {
    @critical int ambient = read_sensor(0);
    int target = 22;
    int mode = 0;
    if (ambient > target) {
        mode = 1;
    } else {
        mode = 2;
    }
    @optional int cache_power = mode * 50;
    checkpoint();
    print(mode);
}`
    },
    {
      id: 'loop_accumulator',
      title: 'Loop-Based Energy Accumulator',
      description: 'Iterative loop accumulating sensor values over multiple cycles before checkpoint preservation.',
      category: 'Loops',
      sourceCode: `program LoopAccumulator {
    @critical int sensor_base = read_sensor(0);
    int sum = 0;
    int i = 0;
    while (i < 5) {
        sum = sum + sensor_base;
        i = i + 1;
    }
    @disposable int loop_counter_debug = i;
    checkpoint();
    print(sum);
}`
    },
    {
      id: 'disposable_debug',
      title: 'Telemetry Logger with Debug State',
      description: 'Edge node with temporary volatile debug state and metadata intended to be discarded across power loss.',
      category: 'Disposable State',
      sourceCode: `program TelemetryLogger {
    @critical int telemetry_packet = read_sensor(2);
    int checksum = telemetry_packet % 256;
    @disposable int log_id = 9999;
    @disposable int volatile_status = 123;
    @disposable int debug_timestamp = 45678;
    checkpoint();
    print(checksum);
}`
    },
    {
      id: 'expensive_recompute',
      title: 'Polynomial Computation Pipeline',
      description: 'Program containing mathematically intensive intermediate polynomial coefficients safe to recompute.',
      category: 'Optimization',
      sourceCode: `program PolynomialPipeline {
    @critical int input_x = read_sensor(0);
    int poly1 = input_x * input_x;
    int poly2 = poly1 * 3;
    int poly3 = poly2 + (input_x * 5);
    int curve_val = poly3 + 17;
    checkpoint();
    int output_metric = curve_val / 4;
    print(output_metric);
}`
    },
    {
      id: 'external_sensor_io',
      title: 'Multi-Sensor Fusion Node',
      description: 'Multiple physical hardware sensor inputs (temperature & pressure) that must strictly be preserved.',
      category: 'Multi-Sensor',
      sourceCode: `program MultiSensorFusion {
    @critical int temp_sensor = read_sensor(0);
    @critical int pressure_sensor = read_sensor(2);
    int combined_index = (temp_sensor * 10) + (pressure_sensor / 100);
    @optional int cache_reading = combined_index;
    checkpoint();
    print(combined_index);
}`
    }
  ];
}

export async function compileCode(sourceCode: string, memoryBudget = 0): Promise<CompilationResult> {
  const res = await fetch(`${API_BASE}/compile`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ sourceCode, memoryBudget }),
  });

  const data = await res.json();
  if (!res.ok || data.error) {
    throw new Error(data.error || `Compiler API error: ${res.statusText}`);
  }

  return data;
}

export async function simulateFailure(
  sourceCode: string,
  strategy: CheckpointStrategy = 'RESILIENCE_AWARE',
  memoryBudget = 0,
  faultCycle = -1,
  faultStatementId = 0
): Promise<SimulationResult> {
  const res = await fetch(`${API_BASE}/simulate`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ sourceCode, strategy, memoryBudget, faultCycle, faultStatementId }),
  });

  const data = await res.json();
  if (!res.ok || data.error) {
    throw new Error(data.error || `Simulation API error: ${res.statusText}`);
  }

  return data;
}

export async function runBenchmarks(sourceCode?: string): Promise<BenchmarkSuiteResult[]> {
  const res = await fetch(`${API_BASE}/benchmark`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ sourceCode: sourceCode || '' }),
  });

  const data = await res.json();
  if (!res.ok || data.error) {
    throw new Error(data.error || `Benchmark API error: ${res.statusText}`);
  }

  return data;
}
