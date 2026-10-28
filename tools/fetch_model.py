#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fetch pinned model assets from ModelScope without installing its runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def fetch(url, target, expected):
    if target.exists():
        if hashlib.sha256(target.read_bytes()).hexdigest() != expected:
            raise ValueError(f"existing file has a different checksum: {target}")
        return "reused"
    temporary = None
    try:
        request = urllib.request.Request(url, headers={"User-Agent": "fsmn-kws/0.1"})
        with urllib.request.urlopen(request, timeout=30) as response:
            with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".download-", delete=False) as output:
                temporary = Path(output.name)
                digest = hashlib.sha256()
                total = 0
                while True:
                    block = response.read(65536)
                    if not block:
                        break
                    total += len(block)
                    if total > 16 * 1024 * 1024:
                        raise ValueError("download exceeds the asset size limit")
                    digest.update(block)
                    output.write(block)
        if digest.hexdigest() != expected:
            raise ValueError(f"download checksum mismatch: {target.name}")
        # Atomic publication without replacing an existing file, including races.
        os.link(temporary, target)
        return "downloaded"
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "models")
    parser.add_argument("--examples", action="store_true", help="also fetch two upstream reference WAVs")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "models/nihaomiya.json").read_text(encoding="utf-8"))
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, asset in manifest["assets"].items():
        if name != "model.txt" and not args.examples:
            continue
        url = urllib.parse.urljoin(manifest["download_base_url"], asset["path"])
        status = fetch(url, args.output_dir / name, asset["sha256"])
        print(json.dumps({"asset": name, "status": status, "sha256": asset["sha256"]}))


if __name__ == "__main__":
    main()
