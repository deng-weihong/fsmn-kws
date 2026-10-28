# Nihaomiya Model Assets

[简体中文](README.md) | **English**

Upstream: [ModelScope iic/speech_dfsmn_kws_char_farfield_16k_nihaomiya](https://www.modelscope.cn/iic/speech_dfsmn_kws_char_farfield_16k_nihaomiya).
The upstream authors trained this model; this project supplies a compatible
runtime and has not retrained or changed its parameters.

`nihaomiya.json` records revision
`3beaea60a917201bd6581f3a39ad4825ff4b2557`, SHA256 checksums, topology and the
upstream model card's Apache License 2.0 declaration. This declaration is
specific to the upstream model repository, not the official binary runtime.

```sh
python3 tools/fetch_model.py
python3 tools/fetch_model.py --examples
```

The script uses Python's standard library. It downloads directly from the
pinned official URLs, verifies SHA256 before exposing a file, reuses matching
files, and rejects existing files with different contents. It does not install
ModelScope, PyTorch, or the official SoundConnect wheel.

Weights and example recordings are ignored by Git and are not part of the source
distribution. Users downloading or redistributing upstream assets should retain
the upstream attribution and applicable license. Do not describe these weights
as trained by FSMN KWS.

Supported format: the original text model containing signed integer encodings
of IEEE-754 FP32 words, with the original fixed topology. The reader checks
defined header fields, finite values, required sizes and trailing content.
