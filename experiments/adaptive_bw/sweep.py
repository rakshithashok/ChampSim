#!/usr/bin/env python3
"""Run and analyze system-level bandwidth/cache sensitivity points."""

import argparse
import copy
import json
import subprocess
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest")
    parser.add_argument("--design", action="append")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    source = json.loads(Path(args.manifest).read_text())
    base_result_dir = source["settings"].get("results_dir", "results/adaptive_bw")
    for point in source.get("sensitivity", []):
        manifest = copy.deepcopy(source)
        manifest["settings"]["system"] = {**manifest["settings"].get("system", {}), **point.get("system", {})}
        manifest["settings"]["results_dir"] = f"{base_result_dir}/sensitivity/{point['name']}"
        generated = ROOT / manifest["settings"]["results_dir"] / "manifest.json"
        generated.parent.mkdir(parents=True, exist_ok=True)
        generated.write_text(json.dumps(manifest, indent=2) + "\n")

        command = ["python3", str(HERE / "run.py"), str(generated)]
        for design in args.design or []:
            command.extend(["--design", design])
        if args.dry_run:
            command.append("--dry-run")
        if args.force:
            command.append("--force")
        subprocess.run(command, cwd=ROOT, check=True)
        if not args.dry_run:
            subprocess.run(["python3", str(HERE / "analyze.py"), str(generated)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
