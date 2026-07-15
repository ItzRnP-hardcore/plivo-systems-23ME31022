# Plivo Systems Assignment: The Flaky Network

**Roll No:** 23ME31022

## Overview

Sender and receiver (C, standard library only) that stream one 160-byte frame every 20ms across a hostile UDP relay (drops, jitter, reordering, duplicates, burst loss) with a deadline-miss rate ≤ 1% and bandwidth overhead ≤ 2.0x.

**Grade at `delay_ms = 120`.**

## Build & Run

```
make
python3 run.py --profile profiles/A.json --delay_ms 120
python3 run.py --profile profiles/B.json --delay_ms 120
```

## Solution Architecture

Hybrid recovery — dual-coverage XOR FEC as the primary defense, NACK retransmission over the feedback lane as a burst backstop:

- **XOR FEC:** ~97% of packets carry `X_i = P(i-2) XOR P(i-K)` (324B packet). Every frame gets two independent repair chances — fast (+2 frames) and burst-safe (+K frames) — for the same 160 bytes plain replication spends on one.
- **Adaptive spacing:** both binaries derive `K = clamp((DELAY_MS - 80)/20, 1, 8)` from the environment, so the protocol automatically uses the deepest burst spacing the graded delay affords. At K ≤ 2 it degrades to plain replication.
- **Lazy NACK:** the receiver NACKs a missing frame only after its fast XOR repair chance has demonstrably failed, re-NACKing every 40ms until the frame's deadline. The sender answers with a duplicated retransmit, capped per seq. Feedback traffic ≈ 90B per 30s run.
- **Deterministic budget:** all relay-bound bytes share one counter; FEC attach gated at 1.975x, retransmits at 1.99x, guaranteeing the 2.0x cap.
- **Stateless playout:** the receiver forwards every payload immediately (the player dedupes; early arrival is free), so no jitter buffer is needed.

## Measured Results (30s runs, seeds 1-3)

| Profile | delay_ms | Miss % | Overhead | Result |
|---------|----------|--------|----------|--------|
| A | 120 | 0.00% | 1.97x | VALID |
| B | 120 | 0.40-0.73% | 1.98x | VALID |
| Gilbert-Elliott burst (self-made) | 120 | 0.60% | 1.97x | VALID |
| Gilbert-Elliott burst (self-made) | 160 | 0.13-0.47% | 1.97x | VALID |

See `RUNLOG.md` for the full experiment log, `NOTES.md` for the design summary, and `SUMMARY.html` for the architecture write-up.
