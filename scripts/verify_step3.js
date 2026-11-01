const http = require('http');

function postCompile(code, budget = 0) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify({ sourceCode: code, memoryBudget: budget });
    const req = http.request('http://localhost:3000/api/compile', {
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
          reject(new Error(`Failed to parse response: ${data}`));
        }
      });
    });
    req.on('error', reject);
    req.write(payload);
    req.end();
  });
}

async function run() {
  const code1 = `program TemperatureMonitor {
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

  const code2 = `program TemperatureMonitorModified {
    @critical int raw = read_sensor(0);
    @critical int emergency_sensor = read_sensor(3);
    int calibration = 3;
    int adjusted = raw + calibration;
    int filtered = adjusted * 2;
    int threshold = 50;
    @optional int cached_status = 1;
    @optional int secondary_cache = 42;
    @disposable int debug_tick = 99;
    checkpoint();
    bool alert = (filtered > threshold) && (emergency_sensor > 10);
    print(alert);
}`;

  console.log('=== Step 3: EdgeLang Input Change Verification ===');
  console.log('\n--- 1. BASELINE INPUT COMPILATION ---');
  const res1 = await postCompile(code1);
  console.log('Program Name:', res1.programName, '| Success:', res1.success);
  console.log('Plan Total Saved Bytes in NVM:', res1.recoveryPlan.totalSavedBytes);
  console.log('Saved Variables in NVM:', res1.recoveryPlan.savedVariables);
  console.log('Recomputed Variables upon Recovery:', res1.recoveryPlan.recomputedVariables);
  console.log('Recomputation Order:', res1.recoveryPlan.recomputeOrder);
  console.log('Variable Classifications:');
  res1.classifications.forEach(c => {
    console.log(`  - ${c.name.padEnd(17)} : ${c.classification.padEnd(14)} (${c.storageSizeBytes}B, recomputable: ${c.isRecomputable}, selected: ${c.isSelectedForCheckpoint})`);
  });

  console.log('\n--- 2. MODIFIED INPUT COMPILATION ---');
  const res2 = await postCompile(code2);
  console.log('Program Name:', res2.programName, '| Success:', res2.success);
  console.log('Plan Total Saved Bytes in NVM:', res2.recoveryPlan.totalSavedBytes);
  console.log('Saved Variables in NVM:', res2.recoveryPlan.savedVariables);
  console.log('Recomputed Variables upon Recovery:', res2.recoveryPlan.recomputedVariables);
  console.log('Recomputation Order:', res2.recoveryPlan.recomputeOrder);
  console.log('Variable Classifications:');
  res2.classifications.forEach(c => {
    console.log(`  - ${c.name.padEnd(17)} : ${c.classification.padEnd(14)} (${c.storageSizeBytes}B, recomputable: ${c.isRecomputable}, selected: ${c.isSelectedForCheckpoint})`);
  });

  // Verify differences
  console.log('\n--- VERIFICATION OF DYNAMIC UPDATES ---');
  const programNameChanged = res1.programName !== res2.programName;
  const newCriticalFound = res2.classifications.some(c => c.name === 'emergency_sensor' && c.classification === 'CRITICAL');
  const newOptionalFound = res2.classifications.some(c => c.name === 'secondary_cache' && c.classification === 'OPTIONAL');
  const savedVariablesUpdated = res2.recoveryPlan.savedVariables.includes('emergency_sensor') && res2.recoveryPlan.savedVariables.includes('secondary_cache');
  const storageIncreased = res2.recoveryPlan.totalSavedBytes > res1.recoveryPlan.totalSavedBytes;

  console.log(`1. Program name changed (${res1.programName} -> ${res2.programName}):`, programNameChanged);
  console.log('2. emergency_sensor classified as CRITICAL:', newCriticalFound);
  console.log('3. secondary_cache classified as OPTIONAL:', newOptionalFound);
  console.log(`4. Saved variables list dynamically updated:`, savedVariablesUpdated);
  console.log(`5. Storage size adjusted (${res1.recoveryPlan.totalSavedBytes}B -> ${res2.recoveryPlan.totalSavedBytes}B):`, storageIncreased);

  if (programNameChanged && newCriticalFound && newOptionalFound && savedVariablesUpdated && storageIncreased) {
    console.log('\n>>> STEP 3 VERIFICATION PASSED: EdgeLang updates dynamically update compiler classifications and recovery plans.');
  } else {
    console.error('\n>>> STEP 3 VERIFICATION FAILED: Updates were not reflected.');
    process.exit(1);
  }
}

run().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
