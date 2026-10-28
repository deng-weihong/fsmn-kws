#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compare compatible reference/candidate CLIs on caller-supplied recordings."""
import argparse
import array
import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

SUFFIXES = {"enhanced.pcm": "h", "features.f32": "f", "gains.f32": "f",
            "context.f32": "f", "posterior.f32": "f", "weights.f32": "f"}
GOLDEN_BLOCKS = {
    "3ad1a268c614076614a2ae6528abc29cc85ae35826d172079d7d9b26a0299559":
        [370, 594, 868, 1158, 1398],
    "8ce83bf2a8e6056aba3b3cdc92d2e04d23bdf15a2c1fde814cb091444d59a10b":
        [312, 470, 610, 744, 892, 1032, 1184, 1310, 1442, 1554],
}
MODEL_SHA256 = "b7a31242882cfbf29bc2d7973e73dc8eb5077c0c4da3fda2cfe43cee8c28be0b"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(binary, model, wav, prefix, equalizer):
    command = [str(binary.resolve()), "--model", str(model.resolve()),
               "--wav", str(wav.resolve()), "--dump-prefix", str(prefix)]
    if equalizer:
        command += ["--input-eq", str(equalizer.resolve())]
    result = subprocess.run(command, capture_output=True, text=True, check=True, timeout=120)
    rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
    events = [{key: row[key] for key in ("block", "confidence", "channel")}
              for row in rows if row.get("type") == "wake"]
    summary = next(row for row in rows if row.get("type") == "summary")
    if summary["blocks"] <= 0:
        raise ValueError("reference regression needs at least one input block")
    return events, summary


def compare_files(left, right, code):
    a, b = left.read_bytes(), right.read_bytes()
    if len(a) != len(b):
        return {"passed": False, "reference_bytes": len(a), "candidate_bytes": len(b)}
    reference, candidate = array.array(code), array.array(code)
    reference.frombytes(a)
    candidate.frombytes(b)
    if sys.byteorder != "little":
        reference.byteswap()
        candidate.byteswap()
    maximum = 0
    finite = True
    for x, y in zip(reference, candidate):
        finite = finite and math.isfinite(x) and math.isfinite(y)
        maximum = max(maximum, abs(float(x) - float(y)))
    return {"values": len(reference), "bitwise_equal": a == b,
            "max_absolute_error": maximum, "passed": a == b and finite}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--wav", type=Path, nargs="+", required=True)
    parser.add_argument("--input-eq", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        parser.error("report already exists; choose a new output path")
    report = {"format": 1, "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "model_sha256": sha256(args.model),
              "reference_binary_sha256": sha256(args.reference),
              "candidate_binary_sha256": sha256(args.candidate),
              "input_equalizer_sha256": sha256(args.input_eq) if args.input_eq else None,
              "scope": "Same-host extraction regression; not a microphone quality benchmark",
              "recordings": []}
    with tempfile.TemporaryDirectory(prefix="fsmn-kws-regression-") as temporary:
        for index, wav in enumerate(args.wav):
            prefixes = [Path(temporary) / f"{index}-{side}" for side in ("reference", "candidate")]
            expected, old_summary = run(args.reference, args.model, wav, prefixes[0], args.input_eq)
            actual, summary = run(args.candidate, args.model, wav, prefixes[1], args.input_eq)
            metrics = {suffix: compare_files(Path(str(prefixes[0]) + "." + suffix),
                                            Path(str(prefixes[1]) + "." + suffix), code)
                       for suffix, code in SUFFIXES.items()}
            digest = sha256(wav)
            golden = GOLDEN_BLOCKS.get(digest) if not args.input_eq and report["model_sha256"] == MODEL_SHA256 else None
            golden_passed = golden is None or [row["block"] for row in actual] == golden
            entry = {"wav_sha256": digest, "blocks": summary["blocks"],
                     "events": actual, "event_parity": expected == actual,
                     "golden_blocks": golden, "golden_passed": golden_passed,
                     "trailing_samples_skipped": summary["trailing_samples_skipped"],
                     "metrics": metrics}
            entry["passed"] = (all(value["passed"] for value in metrics.values())
                               and expected == actual and golden_passed
                               and summary["blocks"] == old_summary["blocks"])
            report["recordings"].append(entry)
    report["passed"] = all(entry["passed"] for entry in report["recordings"])
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("x", encoding="utf-8") as output:
        json.dump(report, output, indent=2, allow_nan=False)
        output.write("\n")
    print(json.dumps({"passed": report["passed"], "recordings": len(report["recordings"]),
                      "events": [len(entry["events"]) for entry in report["recordings"]]}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
