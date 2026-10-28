# 嵌入式 Linux 交叉编译

**简体中文** | [English](cross-compiling.en.md)

运行库不依赖声卡接口、Python 或设备服务。提供 Linux 交叉工具链并关闭宿主机测试即可构建。以 ARMv7 硬浮点 GCC 工具链为例：

```sh
cmake -S . -B build-arm \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER=/path/to/toolchain/bin/arm-linux-gnueabihf-gcc \
  -DCMAKE_CXX_COMPILER=/path/to/toolchain/bin/arm-linux-gnueabihf-g++ \
  -DCMAKE_BUILD_TYPE=Release -DFSMN_KWS_BUILD_TESTS=OFF
cmake --build build-arm -j4
```

请使用实际 SDK 提供的编译器名称和目标 sysroot。工具链要求外部 sysroot 时，传入 `CMAKE_SYSROOT`。RV1106 使用 Rockchip ARMv7-A / Cortex-A7 工具链，典型 SDK 需要以下参数：

```text
-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
```

这些参数应放入工具链文件，或通过 `CMAKE_C_FLAGS`、`CMAKE_CXX_FLAGS` 传入。

`FSMN_KWS_PORTABLE_MATH` 默认开启，用于稳定不同 C 库之间自适应计算的 FP32 幂运算舍入。应保留 `-ffp-contract=off`、关闭 fast-math，以及定点整数回绕相关的编译参数。更换编译器后仍需进行参考对照。

生成的系数头文件使用精确的十六进制 FP32 字面量，因此运行库使用 C++17 编译；安装后的公开接口要求 C++11。

示例使用 POSIX 时钟和资源统计接口，运行于 Linux。应用可设置 `FSMN_KWS_BUILD_EXAMPLES=OFF`，仅链接运行库并自行提供音频采集与 PCM 缓冲。处理格式固定为 16 kHz，每次调用每路恰好 320 个采样。

交叉编译通过并不代表设备验证通过。接入实时麦克风之前，应在目标设备运行相同参考录音，对比增强 PCM、特征、模型概率和事件块号。
