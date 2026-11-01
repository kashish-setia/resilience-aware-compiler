import json
import os
import sys

def main():
    report_path = os.path.join(os.path.dirname(__file__), '..', 'reports', 'benchmark_report.json')
    if not os.path.exists(report_path):
        print(f"Error: Benchmark report not found at {report_path}")
        return 1

    with open(report_path, 'r', encoding='utf-8-sig') as f:
        data = json.load(f)

    print("==========================================================================")
    print("      RESILIENCE-AWARE COMPILER: EMPIRICAL BENCHMARK SUMMARY")
    print("==========================================================================")
    print(f"{'Program':<30} | {'Everything':<10} | {'Liveness':<10} | {'Resilience-Aware':<16} | {'Reduction'}")
    print("-" * 80)

    programs = []
    everything_bytes = []
    liveness_bytes = []
    resilience_bytes = []

    for suite in data:
        name = suite['benchmarkName'][:28]
        comparisons = {c['strategyName']: c for c in suite['comparisons']}
        
        b_every = comparisons.get('CHECKPOINT_EVERYTHING', {}).get('storageBytes', 0)
        b_live = comparisons.get('LIVENESS_ONLY', {}).get('storageBytes', 0)
        c_res = comparisons.get('RESILIENCE_AWARE', {})
        b_res = c_res.get('storageBytes', 0)
        savings = c_res.get('savingsPercent', 0.0)

        programs.append(name)
        everything_bytes.append(b_every)
        liveness_bytes.append(b_live)
        resilience_bytes.append(b_res)

        print(f"{name:<30} | {b_every:>8} B | {b_live:>8} B | {b_res:>14} B | {savings:>6.1f}%")

    print("==========================================================================")

    # Try plotting with matplotlib
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import numpy as np

        x = np.arange(len(programs))
        width = 0.25

        fig, ax = plt.subplots(figsize=(14, 7))
        fig.patch.set_facecolor('#0f172a')
        ax.set_facecolor('#1e293b')

        rects1 = ax.bar(x - width, everything_bytes, width, label='Checkpoint Everything', color='#64748b')
        rects2 = ax.bar(x, liveness_bytes, width, label='Liveness-Only', color='#38bdf8')
        rects3 = ax.bar(x + width, resilience_bytes, width, label='Resilience-Aware (Our Compiler)', color='#34d399')

        ax.set_ylabel('Checkpoint Storage (Bytes)', color='#f8fafc', fontsize=12)
        ax.set_title('Checkpoint Memory Reduction Across 9 Edge Systems Benchmarks', color='#f8fafc', fontsize=14, fontweight='bold', pad=15)
        ax.set_xticks(x)
        ax.set_xticklabels(programs, rotation=35, ha='right', color='#e2e8f0', fontsize=9)
        ax.tick_params(colors='#94a3b8')
        ax.grid(axis='y', linestyle='--', alpha=0.25, color='#cbd5e1')
        ax.legend(facecolor='#0f172a', edgecolor='#334155', labelcolor='#f8fafc')

        output_img = os.path.join(os.path.dirname(__file__), '..', 'reports', 'benchmark_comparison.png')
        plt.tight_layout()
        plt.savefig(output_img, dpi=200, facecolor=fig.get_facecolor(), edgecolor='none')
        print(f"\n[Visualizer] High-resolution chart saved to: {output_img}")
    except ImportError:
        print("\n[Visualizer] Matplotlib not available; textual metrics generated successfully.")

    return 0

if __name__ == '__main__':
    sys.exit(main())
