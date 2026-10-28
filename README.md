# FSMN KWS

**简体中文** | [English](README.en.md)

面向嵌入式 Linux 的 C/C++ 声学前端与离线语音唤醒运行库。当前实现独立重建了 Miya 的固定默认处理链路，使用 ModelScope 原始 **“你好米雅”（Nihaomiya）** 模型，在 CPU 上执行 FP32 推理。

声学前端可以脱离模型单独使用。模型权重从上游单独下载；运行库使用仓库内保留的 DSP 依赖源码离线构建，运行时不加载官方 SoundConnect 库。实现来源及许可核实状态见[来源说明](docs/provenance.md)。

## 构建与测试

环境要求：Linux、GCC 或 Clang、CMake >= 3.16，以及支持 C++17 的编译器。公开头文件可供 C++11 应用使用。下载资源和运行命令行集成测试时，可选使用 Python >= 3.8。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
python3 tools/check_sources.py
```

测试使用生成信号和合成模型权重，不需要下载模型、安装第三方 Python 包，也不需要麦克风、网络或相邻工程。未安装 Python 时会跳过命令行集成测试，C++ 测试仍可运行。

## 检测唤醒词

下载固定版本的上游模型及可选参考录音：

```sh
python3 tools/fetch_model.py --examples
build/kws_wav --model models/model.txt --wav models/3ch_nihaomiya.wav
```

两段参考 WAV 的基准检出次数分别为 5 次和 10 次。命令行逐行输出 JSON，包含输入块号、时间、HMM 分数和选中的麦克风，最后输出耗时与内存统计。已验证范围见[验证说明](docs/validation.md)。

从 ALSA 采集设备实时检测：

```sh
set -o pipefail
arecord -q -D default -t raw -f S16_LE -r 16000 -c 2 \
  | build/kws_stream --model models/model.txt --channels 2
```

两路麦克风加同步播放参考时使用 `--channels 3`。`--channels 1` 会复制单路麦克风输入，属于兼容模式，其效果不能视为真实双麦阵列的验证结果。

## 单独使用声学前端

无需下载模型，即可增强双通道或三通道录音：

```sh
build/frontend_wav --wav raw_input.wav --output enhanced.wav
```

输出为双声道 16 kHz PCM16 WAV；若输出文件已存在，工具会拒绝覆盖。所有 WAV 工具均要求 16 kHz PCM16 输入，通道顺序为麦克风 0、麦克风 1、可选的播放参考。末尾不足一个完整块的采样会被跳过，并在结果中记录。

## C++ 接口

```cpp
#include <fsmn_kws/frontend.h>
#include <fsmn_kws/engine.h>

fsmn_kws::Frontend frontend;
fsmn_kws::Engine wake("models/model.txt", 0.81f);

// 每路输入均指向恰好 320 个连续 PCM16 采样。
auto audio = frontend.step(mic0, mic1, playback_reference);
auto result = wake.step(mic0, mic1, playback_reference);
if (result.detected) {
    on_keyword(result.confidence, result.channel);
}
```

仅需增强音频或特征时使用 `Frontend`，完整唤醒使用 `Engine`。`Engine` 内部已包含前端。每个实例对应一条音频流，同一时刻只能由一个线程调用。数据布局、时序、流中断处理及可选输入校正见[接口约定](docs/api.md)。

安装后可通过 CMake 引用：

```sh
cmake --install build --prefix "$PWD/build/install"
```

```cmake
find_package(fsmn_kws 0.1 CONFIG REQUIRED)
target_link_libraries(your_application PRIVATE fsmn_kws::fsmn_kws)
```

将 `CMAKE_PREFIX_PATH` 指向安装前缀。安装包包含公开头文件、静态库、许可声明和依赖来源记录。使用嵌入式 Linux SDK 构建的方法见[交叉编译说明](docs/cross-compiling.md)。

## 目录与支持范围

| 目录 | 内容 |
| --- | --- |
| `include/fsmn_kws` | 独立声学前端与完整唤醒引擎接口 |
| `src/frontend` | 高通、PPFbank、DRAEC、AuxIVA、AGC、Fbank 及默认参数 |
| `src/model`、`src/decoder` | 固定拓扑 FSMN 与自适应 HMM |
| `third_party` | 固定版本的 KissFFT、WebRTC 数字 AGC 和 musl 数学源码 |
| `examples` | WAV 检测、PCM 流式检测与 WAV 增强示例 |
| `tests` | 接口、流状态、模型格式与命令行约定的离线测试 |
| `tools`、`models`、`docs` | 源码校验、资源清单与文档 |

首版支持原始五分类 Nihaomiya 模型拓扑和固定默认前端参数。自定义唤醒词、任意 FSMN 架构、模型训练、非默认算法和设备服务部署不在本版范围内。

参考录音数值一致性与真实麦克风召回率、每小时误唤醒次数是不同的验证项目。设备 EQ 必须针对实际声学路径测量。

## 许可证与署名

本项目贡献采用 [Apache License 2.0](LICENSE)。第三方代码保留各自的许可证、版权声明及专利授权，见[第三方声明](THIRD_PARTY_NOTICES.md)。模型权重来自上游训练成果，并非由本项目训练，其来源和声明的许可证见[模型说明](models/README.md)。
