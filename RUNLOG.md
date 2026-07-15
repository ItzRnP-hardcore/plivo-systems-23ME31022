# Systems Handout RUNLOG

| Profile | Offset | Delay (ms) | Seed | Miss % | Overhead (x) | Result | Notes |
|---------|--------|------------|------|--------|--------------|--------|-------|
| B.json  | 1      | 100        | 1    | 1.35%  | 1.98x        | INVALID| Baseline offset 1 (fails on bursts) |
| B.json  | 1      | 100        | 2    | 1.28%  | 1.98x        | INVALID| |
| B.json  | 1      | 100        | 3    | 1.40%  | 1.98x        | INVALID| |
| B.json  | 1      | 100        | 4    | 1.30%  | 1.98x        | INVALID| |
| B.json  | 1      | 100        | 5    | 1.37%  | 1.98x        | INVALID| |
| B.json  | 2      | 120        | 1    | 0.53%  | 1.98x        | VALID  | Offset 2 logic (burst resilient) |
| B.json  | 2      | 120        | 2    | 0.60%  | 1.98x        | VALID  | |
| B.json  | 2      | 120        | 3    | 0.93%  | 1.98x        | VALID  | |
| B.json  | 2      | 120        | 4    | 0.47%  | 1.98x        | VALID  | |
| B.json  | 2      | 120        | 5    | 0.67%  | 1.98x        | VALID  | |

### Experiment Analysis
The goal was to test Offset 1 vs Offset 2 under Profile B. Profile B is harsher with max jitter of 80ms.
- For **Offset 1**, the repair packet arrives in the next frame (+20ms). Adding the max jitter of 80ms, the minimum safe playout delay is `20 + 80 = 100ms`.
- For **Offset 2**, the repair packet arrives two frames later (+40ms). The minimum safe playout delay is `40 + 80 = 120ms`.

Offset 1 provides lower delay but is extremely vulnerable to burst losses of length 2 (which wipe out both `i` and its backup in `i+1`).
Offset 2 trades 20ms of delay for resilience against burst losses of length 2. The data shows Offset 2 is strictly required to pass Profile B's burst characteristics, dropping the miss rate safely below the 1% cap at exactly 1.98x overhead.
