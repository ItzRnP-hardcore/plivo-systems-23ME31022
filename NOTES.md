# Design Notes

My solution implements a Forward Error Correction (FEC) mechanism using Delayed Replication to completely bypass the latency penalty of ARQ retransmissions. To maximize redundancy coverage within the strict $2.0x$ bandwidth limit, the sender statically gates redundancy: if appending a backup payload keeps the running overhead below $1.98x$, it appends a previous payload. The choice of which history payload to attach is dictated by an `OFFSET` macro. 
I tested two offsets: Offset 1 (backing up frame $i-1$) and Offset 2 (backing up frame $i-2$). While Offset 1 yields a slightly lower theoretical minimum delay (100ms vs 120ms), it is fundamentally susceptible to burst losses of length 2, which drop both a packet and its immediate backup. The Flaky Network (especially Profile B) contains burst loss characteristics where double-loss probability significantly exceeds independent packet drops, meaning Offset 1 struggles to consistently keep the deadline miss rate below 1%. 
Offset 2 perfectly combats bursts up to length 2 by spacing the backup packet far enough out to survive short disruptions. The cost is an extra 20ms of jitter buffer depth.

**Grade at: `delay_ms` = 120**

What breaks it: Extended burst losses lasting $\ge 3$ consecutive packets ($\ge 60ms$ outage) will overwhelm the Offset 2 redundancy. Severe bandwidth starvation forcing overhead below 1.5x would also drastically reduce redundancy coverage, driving up the miss rate.
