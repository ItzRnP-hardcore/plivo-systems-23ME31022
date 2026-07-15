# RUNLOG

All runs: 30s duration (1500 frames), overhead cap 2.0x, miss cap 1.0%.
Profiles: A.json / B.json from the handout. D.json is a self-made burst-loss
stress profile (`loss 0.02, jitter 20-80ms, dup 0.01, Gilbert-Elliott
p_enter 0.01, p_exit 0.5, p_loss_in_burst 0.8`, ~3.5% effective loss with
multi-packet bursts) used to test burst behavior the handout profiles lack.

## Phase 1 — plain replication FEC (payload i-OFFSET piggybacked, no feedback)

| Profile | Offset | Delay | Seed | Miss % | Overhead | Result | Notes |
|---------|--------|-------|------|--------|----------|--------|-------|
| B | 2 | 120 | 1 | 0.53% | 1.98x | VALID | baseline design |
| B | 2 | 120 | 2 | 0.47% | 1.98x | VALID | |
| B | 2 | 120 | 3 | 0.80% | 1.98x | VALID | |
| A | 2 | 120 | 1 | 0.13% | 1.98x | VALID | |
| B | 2 | 110 | 1 | 1.47% | 1.98x | INVALID | repair (+40ms) misses deadline when jitter > 70ms; 120 is the floor for offset 2 |
| B | 1 | 100 | 1 | 0.60% | 1.98x | VALID | A/B loss is i.i.d., so offset 1 matches offset 2's miss rate at 20ms less delay |
| B | 1 | 100 | 2 | 0.40% | 1.98x | VALID | |
| B | 1 | 100 | 3 | 0.67% | 1.98x | VALID | |
| D | 2 | 120 | 1 | ~2-5% | 1.98x | INVALID | single repair copy dies in the same burst; replication alone cannot fix bursts |
| D | 4 | 160 | 1 | 3.40% | 1.98x | INVALID | deeper offset helps too slowly against fat-tailed burst lengths |

**Lesson:** replication is fine for i.i.d. loss but a single repair copy is
correlated with its original under burst loss. Changed to (a) XOR FEC giving
every frame two independent repair chances for the same bytes, and (b) NACK
retransmission over the previously unused feedback lane.

## Phase 2 — hybrid: dual-coverage XOR FEC + NACK (final design)

An intermediate version NACKed every gap immediately: the NACK storm's
retransmits displaced the FEC budget (up_pkts 2424, coverage fell to ~31%,
miss 1.87% on D@160). Fixed by NACKing only frames whose fast XOR repair
chance has already failed, plus a per-seq retransmit cap.

| Profile | Delay (K auto) | Seed | Miss % | Overhead | Result |
|---------|----------------|------|--------|----------|--------|
| B | 120 (K=2) | 1 | 0.53% | 1.98x | VALID |
| B | 120 (K=2) | 2 | 0.40% | 1.98x | VALID |
| B | 120 (K=2) | 3 | 0.73% | 1.98x | VALID |
| A | 120 (K=2) | 1 | 0.00% | 1.97x | VALID |
| D | 120 (K=2) | 1 | 0.60% | — | VALID |
| D | 140 (K=3) | 1 | 0.40% | — | VALID |
| D | 160 (K=4) | 1 | 0.40% | 1.97x | VALID |
| D | 160 (K=4) | 2 | 0.40% | — | VALID |
| D | 160 (K=4) | 3 | 0.13% | — | VALID |

Feedback traffic is negligible (~90B per run ≈ 23 NACKs), so overhead is
effectively the FEC gate (1.975x target, 1.97-1.98x measured).

**Locked: grade at delay_ms = 120.** Valid on A, B, and the burst stress
profile; deeper burst protection scales automatically if graded at a higher
delay (K derives from DELAY_MS).
