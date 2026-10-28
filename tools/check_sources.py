#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify vendored dependency files against their recorded upstream hashes."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    count = 0
    for component in ("kissfft", "webrtc_agc", "musl_math"):
        folder = ROOT / "third_party" / component
        manifest = json.loads((folder / "PROVENANCE.json").read_text(encoding="utf-8"))
        for entry in manifest.get("files", manifest.get("sources", [])):
            expected = entry.get("vendored_sha256", entry.get("sha256"))
            actual = hashlib.sha256((folder / entry["file"]).read_bytes()).hexdigest()
            if actual != expected:
                raise ValueError(f"source checksum mismatch: {component}/{entry['file']}")
            count += 1
    print(json.dumps({"verified_dependency_files": count}))


if __name__ == "__main__":
    main()
