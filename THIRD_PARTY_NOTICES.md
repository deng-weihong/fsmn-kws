# 第三方声明

**简体中文** | [English](THIRD_PARTY_NOTICES.en.md)

本项目自身贡献采用 Apache License 2.0。第三方文件保留原有版权声明和许可证原文。

| 组件 | 许可证 | 保留的声明 | 修改情况 |
| --- | --- | --- | --- |
| KissFFT 131.1.0 | BSD-3-Clause | `third_party/kissfft/COPYING`、`LICENSES/BSD-3-Clause` | 上游文件未修改，频谱打包包装层单独实现 |
| WebRTC audio processing 0.3 数字 AGC | BSD-3-Clause 及随附专利授权 | `third_party/webrtc_agc/COPYING`、`AUTHORS`、`webrtc/PATENTS` | 数字 AGC 已适配，修改记录见其 README 和来源清单 |
| musl / Arm FP32 数学实现 | MIT 及各文件保留的声明 | `third_party/musl_math/COPYRIGHT` 和源码文件头 | 选取的上游文件未修改，另有本地适配代码和符号前缀 |

每个依赖均包含 `PROVENANCE.json`，记录来源地址和校验值。运行 `python3 tools/check_sources.py` 可验证保留的上游文件。

Nihaomiya 模型不包含在源码发布包中。其上游模型卡声明采用 Apache License 2.0，固定版本和署名见[模型说明](models/README.md)。模型许可与本运行库分开处理。

仓库未包含官方 SoundConnect 共享库、wheel、个人录音或设备专用校正参数。兼容性说明及尚需核实的上游许可事项见[来源说明](docs/provenance.md)；模型卡不能作为官方运行库或兼容实现的许可依据。
