# Plivo Systems Assignment: The Flaky Network
**Roll No:** 23ME31022

## Overview
This repository contains the solution for Plivo's "The Flaky Network" assignment, focusing on building a highly resilient UDP packet streaming system under strict latency and bandwidth constraints.

## Solution Architecture
The implementation bypasses the inherent latency penalties of standard ARQ (retransmission) protocols by utilizing **Forward Error Correction (FEC) via Delayed Replication**. 

The system aggressively duplicates payloads up to a strict $2.0x$ bandwidth threshold, scheduling backup packets with a calculated `OFFSET` to perfectly defeat short burst losses characteristic of unreliable internet connections.

### Key Features:
- **Stateless Receiver Routing:** Avoids complex jitter buffers and lock contention by fully delegating packet deduplication to the harness logic.
- **Bandwidth Strictness:** Dynamically gates redundancy, precisely capping overhead at $1.98x$ coverage (preventing automatic invalidation).
- **Burst Immunity:** Optimized with `OFFSET=2` to ensure that small burst packet drops do not permanently wipe out both primary and redundant payloads.

## Validation Results
Achieved a consistently `VALID` benchmark rating for Profile B with max jitter of 80ms:
- **`delay_ms`** configured to `120`
- **Deadline Misses** consistently maintained below the strict `< 1%` threshold
- **Overhead Cap** consistently maintained safely below the `< 2.0x` ceiling
