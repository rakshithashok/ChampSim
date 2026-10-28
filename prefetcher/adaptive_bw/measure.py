#!/usr/bin/env python3
"""Summarize ChampSim --json results from matched prefetcher runs.

Usage: measure.py --no no.json --adaptive adaptive.json [--stride stride.json ...]
All files must use the same trace, warmup, simulation length, and base config.
"""

import argparse
import json
from pathlib import Path


def read_roi(path):
    phases = json.loads(Path(path).read_text())
    if len(phases) != 1:
        raise ValueError(f"{path}: expected one measured phase, found {len(phases)}")
    return phases[0]["roi"]


def demand_misses(roi, cache="L2C"):
    stats = roi[cache]
    return sum(sum(stats[k]["miss"]) for k in ("LOAD", "RFO", "WRITE"))


def dram_reads(roi):
    return sum(ch["RQ ROW_BUFFER_HIT"] + ch["RQ ROW_BUFFER_MISS"] for ch in roi["DRAM"])


def metrics(path, no_misses=None, no_reads=None):
    roi = read_roi(path)
    core = roi["cores"][0]
    instrs, cycles = core["instructions"], core["cycles"]
    l2 = roi["L2C"]
    issued = sum(l2["prefetch issued"])
    useful = sum(l2["useful prefetch"])
    misses = demand_misses(roi)
    reads = dram_reads(roi)
    return {
        "IPC": instrs / cycles if cycles else 0.0,
        "MPKI": 1000.0 * misses / instrs if instrs else 0.0,
        "prefetch_accuracy": useful / issued if issued else 0.0,
        "coverage": (1.0 - misses / no_misses) if no_misses else None,
        "bandwidth_overhead": (100.0 * (reads / no_reads - 1.0)) if no_reads else None,
        "issued": issued,
        "useful": useful,
        "demand_misses": misses,
        "dram_read_requests": reads,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no", required=True, help="JSON result for no prefetching")
    parser.add_argument("--adaptive", required=True, help="JSON result for adaptive_bw")
    parser.add_argument("--stride", help="JSON result for ip_stride")
    parser.add_argument("--baseline", action="append", default=[], metavar="NAME=JSON", help="Additional prefetcher result")
    args = parser.parse_args()

    inputs = {"no": args.no, "adaptive_bw": args.adaptive}
    if args.stride:
        inputs["ip_stride"] = args.stride
    for item in args.baseline:
        name, sep, path = item.partition("=")
        if not sep or not name or not path:
            parser.error(f"invalid --baseline {item!r}; use NAME=JSON")
        inputs[name] = path

    baseline = read_roi(args.no)
    no_misses, no_reads = demand_misses(baseline), dram_reads(baseline)
    print("prefetcher,IPC,MPKI,prefetch_accuracy,coverage,bandwidth_overhead_pct,issued,useful,demand_misses,dram_read_requests")
    for name, path in inputs.items():
        row = metrics(path, no_misses, no_reads)
        vals = [name, f"{row['IPC']:.6f}", f"{row['MPKI']:.6f}", f"{row['prefetch_accuracy']:.6f}"]
        vals += ["NA" if row["coverage"] is None else f"{row['coverage']:.6f}"]
        vals += ["NA" if row["bandwidth_overhead"] is None else f"{row['bandwidth_overhead']:.3f}"]
        vals += [str(row[k]) for k in ("issued", "useful", "demand_misses", "dram_read_requests")]
        print(",".join(vals))


if __name__ == "__main__":
    main()
