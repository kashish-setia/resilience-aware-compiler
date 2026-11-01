const http = require('http');

function postSimulate(opts) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(opts);
    const req = http.request('http://localhost:3000/api/simulate', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(payload)
      }
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          resolve(JSON.parse(data));
        } catch (e) {
          reject(new Error(`Failed to parse simulate response: ${data}`));
        }
      });
    });
    req.on('error', reject);
    req.write(payload);
    req.end();
  });
}

async function run() {
  const baseCode = `program TemperatureMonitor {
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
}`;

  const explicitFaultCode = `program ExplicitFaultMonitor {
    @critical int raw = read_sensor(0);
    int calibration = 3;
    int adjusted = raw + calibration;
    int filtered = adjusted * 2;
    int threshold = 50;
    @optional int cached_status = 1;
    checkpoint();
    fault("brownout_drop_below_1.8V");
    bool alert = filtered > threshold;
    print(alert);
}`;

  console.log('================================================================');
  console.log('  Step 4: Failure Simulation & Correctness Oracle Verification  ');
  console.log('================================================================\n');

  const testCases = [
    {
      name: 'Point 1: Transient power loss immediately following checkpoint',
      code: baseCode,
      strategy: 'RESILIENCE_AWARE',
      faultCycle: -1,
      faultStatementId: 0
    },
    {
      name: 'Point 2: Fault injected at virtual cycle 12',
      code: baseCode,
      strategy: 'RESILIENCE_AWARE',
      faultCycle: 12,
      faultStatementId: 0
    },
    {
      name: 'Point 3: Fault injected at virtual cycle 15',
      code: baseCode,
      strategy: 'RESILIENCE_AWARE',
      faultCycle: 15,
      faultStatementId: 0
    },
    {
      name: 'Point 4: Explicit program fault statement ("brownout_drop_below_1.8V")',
      code: explicitFaultCode,
      strategy: 'RESILIENCE_AWARE',
      faultCycle: -1,
      faultStatementId: 0
    },
    {
      name: 'Point 5: Comparative check - MANUAL_ANNOTATION under same fault',
      code: baseCode,
      strategy: 'MANUAL_ANNOTATION',
      faultCycle: -1,
      faultStatementId: 0
    }
  ];

  let passedPoints = 0;

  for (let i = 0; i < testCases.length; i++) {
    const tc = testCases[i];
    console.log(`[TEST ${i + 1}] ${tc.name}`);
    console.log(`  Strategy: ${tc.strategy} | FaultCycle: ${tc.faultCycle} | FaultStmtId: ${tc.faultStatementId}`);

    const sim = await postSimulate({
      sourceCode: tc.code,
      strategy: tc.strategy,
      memoryBudget: 0,
      faultCycle: tc.faultCycle,
      faultStatementId: tc.faultStatementId
    });

    console.log(`  Failure Point Description : "${sim.failurePointDescription}"`);
    console.log(`  Reference Outputs         : [${sim.referenceOutputs.join(', ')}]`);
    console.log(`  Recovered Outputs         : [${sim.recoveredOutputs.join(', ')}]`);
    console.log(`  Checkpoint NVM Size       : ${sim.checkpointSizeBytes} bytes (vs baseline ${sim.baselineSizeBytes} bytes)`);
    console.log(`  Variables Saved / Recomp  : ${sim.savedVarCount} saved, ${sim.recomputedVarCount} recomputed`);
    console.log(`  Correctness Oracle Result : ${sim.isCorrect ? 'PASSED (100% MATCH)' : 'FAILED (CORRUPTED / DIVERGED)'}`);

    // Log key recovery events
    const recomputeEvents = sim.events.filter(e => e.phase === 'RECOMPUTE');
    if (recomputeEvents.length > 0) {
      console.log('  Recompute Actions:');
      recomputeEvents.forEach(e => console.log(`    * [Cycle ${e.cycle}] ${e.message}`));
    }

    if (tc.strategy === 'RESILIENCE_AWARE') {
      if (sim.isCorrect && sim.recoveredOutputs.length > 0 && JSON.stringify(sim.recoveredOutputs) === JSON.stringify(sim.referenceOutputs)) {
        console.log(`  -> RESULT: SUCCESS (Output perfectly preserved across power crash)\n`);
        passedPoints++;
      } else {
        console.error(`  -> RESULT: FAILURE (Output mismatch or crash!)\n`);
      }
    } else if (tc.strategy === 'MANUAL_ANNOTATION') {
      if (sim.isCorrect && sim.recoveredOutputs.length > 0 && JSON.stringify(sim.recoveredOutputs) === JSON.stringify(sim.referenceOutputs)) {
        console.log(`  -> RESULT: SUCCESS (Output perfectly preserved across power crash via dependency recomputation!)\n`);
        passedPoints++;
      } else {
        console.error(`  -> RESULT: FAILURE (Manual annotation recovery failed: output mismatch or uninitialized state!)\n`);
      }
    }
  }

  console.log(`================================================================`);
  console.log(`  Failure Simulation Summary: ${passedPoints} / ${testCases.length} Scenarios Verified`);
  console.log(`================================================================`);

  if (passedPoints === testCases.length) {
    console.log('>>> STEP 4 VERIFICATION PASSED: Recovery correctness confirmed across multiple failure injection points.');
  } else {
    console.error('>>> STEP 4 VERIFICATION FAILED.');
    process.exit(1);
  }
}

run().catch(err => {
  console.error('Fatal error in simulation:', err);
  process.exit(1);
});
