const { execSync } = require('child_process');

const strats = ['CHECKPOINT_EVERYTHING', 'LIVENESS_ONLY', 'MANUAL_ANNOTATION', 'RESILIENCE_AWARE'];
console.log('========================================================================================');
console.log('             SIMULATION VERIFICATION OF ALL 4 RECOVERY STRATEGIES');
console.log('========================================================================================');

strats.forEach(s => {
  const out = execSync(`build\\edgec.exe --simulate examples\\temperature_logger.edge --strategy ${s}`).toString();
  const res = JSON.parse(out);
  console.log(`[${s.padEnd(22)}] Correct: ${String(res.isCorrect).padEnd(5)} | Size: ${String(res.checkpointSizeBytes).padStart(2)}B | Saved: ${res.savedVarCount} | Recomputed: ${res.recomputedVarCount} | Out: [${res.recoveredOutputs.join(', ')}]`);
});

console.log('========================================================================================\n');
