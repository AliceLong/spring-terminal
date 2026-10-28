# 本地园丁决策

当前使用 `director.py`、`observation.py` 和 `device_bridge.py` 提供低频生活选择、表达校验与有边界的USB请求。模型接口默认为127.0.0.1:1234，默认模型标识qwen3.5-4b。模型权重不包含在本包。

设备桥接由状态服务的 `--mind-bridge` 显式启用，真实回执决定到达/完成，发送请求不等于完成。经济变更仍由设备规则及明确批准控制。`mind.py`、`fixtures.py` 等保留早期原型与回放用途，以当前 director/bridge 为主路径。

运行单元测试前先运行根目录 `tools/test-submission.py`，它会从 C++ 生成合成测试数据。完整开机/模型/语音效果不由这些离线测试替代。
