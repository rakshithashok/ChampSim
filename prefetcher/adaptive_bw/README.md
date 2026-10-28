# `adaptive_bw`

An IP-indexed sequential/stride prefetcher with a bounded lookahead distance.
It raises the distance when useful-prefetch feedback is good and the MSHR and
prefetch queues are lightly occupied, and reduces it under queue pressure or
when fills are evicted unused. Prefetches stay within the current page unless
the cache uses virtual prefetching.

Select it in a ChampSim configuration with:

```json
"prefetcher": "adaptive_bw"
```

The module prints issued, useful, late and unused counts and the final distance
at simulation end. ChampSim's `--json` output supplies the counters for IPC,
MPKI, prefetch accuracy, coverage and DRAM read traffic. After running matched
simulations with JSON output, summarize them with:

```sh
python3 prefetcher/adaptive_bw/measure.py \
  --no results/no.json \
  --adaptive results/adaptive_bw.json \
  --stride results/ip_stride.json \
  --baseline next_line=results/next_line.json \
  --baseline spp_dev=results/spp_dev.json
```

For each run, select the prefetcher on the same cache (the script uses L2C),
and keep the trace, warmup, simulation length and all other configuration
settings identical. Save each result using ChampSim's `--json` option. The
script reports IPC, L2 demand MPKI, useful/issued accuracy, and coverage as
`1 - candidate L2 demand misses / no-prefetch L2 demand misses`. Bandwidth
overhead is the percentage change in DRAM read requests (row-buffer hits plus
misses) from the no-prefetch run. This request count includes demand traffic;
the difference estimates extra traffic and can be negative if prefetching
reduces later demand misses.

Queue occupancy is used as a live local proxy for bandwidth pressure because
the prefetcher interface does not expose measured memory bandwidth. The
reported accuracy from the script uses ChampSim's cache-level useful and
issued counters; the module's late and unused counts are additional diagnostics.
