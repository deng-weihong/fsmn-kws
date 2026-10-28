# Nihaomiya 模型资源

**简体中文** | [English](README.en.md)

上游：[ModelScope iic/speech_dfsmn_kws_char_farfield_16k_nihaomiya](https://www.modelscope.cn/iic/speech_dfsmn_kws_char_farfield_16k_nihaomiya)。模型由上游作者训练，本项目提供兼容运行库，没有重新训练或修改模型参数。

`nihaomiya.json` 记录了固定版本 `3beaea60a917201bd6581f3a39ad4825ff4b2557`、SHA256 校验值、模型拓扑，以及上游模型卡中声明的 Apache License 2.0。该声明针对上游模型仓库，不代表官方二进制运行库的许可。

```sh
python3 tools/fetch_model.py
python3 tools/fetch_model.py --examples
```

脚本仅使用 Python 标准库，直接从固定版本的官方地址下载。文件对外可见前会校验 SHA256；已有文件内容一致时复用，内容不一致时拒绝覆盖。脚本不会安装 ModelScope、PyTorch 或官方 SoundConnect wheel。

权重和示例录音已被 Git 忽略，不属于源码发布包。下载或再分发上游资源时，应保留上游署名和适用许可证，不应将这些权重描述为由 FSMN KWS 训练。

支持格式为原始文本模型：使用有符号整数编码 IEEE-754 FP32 数据，并保留原始固定拓扑。读取器会检查已定义的头部字段、数值有限性、所需数据长度以及尾部多余内容。
