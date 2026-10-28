#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Offline CLI contracts using generated signals and synthetic model weights."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import wave


def model_bytes():
    headers = [[0, 1, 120, 5, 3], [1, 0, 120, 144, 1, 1],
               [5, 0, 144, 68, 16, 1, 5, 4], [1, 1, 144, 5, 1, 4]]
    values = [float(v) for row in headers for v in row + [0] * (10 - len(row))]
    values += [0.0] * (144 * 120 + 144)
    values += [0.0] * (5 * (68 * 144 + 16 * 68 + 68 + 144 * 68 + 144))
    values += [0.0] * (5 * 144 + 8)
    return "".join(f"{struct.unpack('<i', struct.pack('<f', v))[0]}\n" for v in values)


def run(command, *, data=None, success=True):
    result = subprocess.run(command, input=data, capture_output=True, timeout=30)
    if (result.returncode == 0) != success:
        raise AssertionError((command, result.returncode, result.stderr.decode()))
    return [json.loads(line) for line in result.stdout.splitlines() if line.startswith(b"{")]


def main():
    parser = argparse.ArgumentParser()
    for name in ("wav", "stream", "frontend"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    for binary in (args.wav, args.stream, args.frontend):
        run([binary, "--help"])
    with tempfile.TemporaryDirectory(prefix="fsmn-kws-cli-") as temporary:
        root = Path(temporary)
        model = root / "synthetic.txt"
        model.write_text(model_bytes(), encoding="ascii")
        source = root / "input.wav"
        pcm = b"\0" * (320 * 2 * 2 * 12 + 7 * 2 * 2)
        with wave.open(str(source), "wb") as output:
            output.setparams((2, 2, 16000, 0, "NONE", "not compressed"))
            output.writeframes(pcm)
        prefix = root / "dump"
        rows = run([args.wav, "--model", str(model), "--wav", str(source),
                    "--dump-prefix", str(prefix)])
        assert rows[-1]["blocks"] == 12 and rows[-1]["trailing_samples_skipped"] == 7
        assert rows[-1]["events"] == 0
        expected = {"enhanced.pcm": 12 * 640 * 2, "features.f32": 12 * 80 * 4,
                    "gains.f32": 12 * 2 * 4, "context.f32": 6 * 240 * 4,
                    "posterior.f32": 6 * 5 * 4, "weights.f32": 6 * 2 * 4}
        for suffix, size in expected.items():
            assert Path(str(prefix) + "." + suffix).stat().st_size == size, suffix
        rows = run([args.stream, "--model", str(model), "--channels", "2"], data=pcm)
        assert rows[-1]["blocks"] == 12 and rows[-1]["trailing_bytes_skipped"] == 28
        run([args.stream, "--model", str(model)], data=b"", success=False)
        run([args.stream, "--model", str(model)], data=pcm[:1], success=False)
        run([args.stream, "--model", str(model), "--channels", "4"], success=False)
        run([args.stream, "--model", str(model), "--level", "nan"], success=False)
        output = root / "enhanced.wav"
        command = [args.frontend, "--wav", str(source), "--output", str(output)]
        rows = run(command)
        assert rows[-1]["blocks"] == 12
        with wave.open(str(output), "rb") as enhanced:
            assert (enhanced.getnchannels(), enhanced.getframerate(), enhanced.getnframes()) == (2, 16000, 3840)
            assert enhanced.readframes(3840) == Path(str(prefix) + ".enhanced.pcm").read_bytes()
        before = output.read_bytes()
        run(command, success=False)
        assert output.read_bytes() == before
        before = source.read_bytes()
        run([args.frontend, "--wav", str(source), "--output", str(source)], success=False)
        assert source.read_bytes() == before
        invalid = root / "invalid.wav"
        invalid.write_bytes(b"RIFF")
        run([args.wav, "--model", str(model), "--wav", str(invalid)], success=False)
        mono = root / "mono.wav"
        with wave.open(str(mono), "wb") as output:
            output.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
            output.writeframes(b"\0" * 640)
        run([args.wav, "--model", str(model), "--wav", str(mono)], success=False)
    print("WAV, PCM stream, output protection and invalid-input contracts passed")


if __name__ == "__main__":
    main()
