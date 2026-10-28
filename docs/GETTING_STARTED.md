# 运行、构建与验证

以下命令均从仓库根目录执行。首次固件构建需要联网获取锁定的第三方依赖，ESP-IDF SDK 与本地大模型权重不在仓库内。

## 运行电脑终端

需要 Python 3.10 或更新版本。固件构建使用 ESP-IDF 自带 Python 环境，两者可以分开。

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
python monitor/server.py
```

USB 数据线连接设备后访问 `http://127.0.0.1:8766`。默认命令只观察串口，不启用模型桥接或情境切换。多设备时加 `--serial /dev/cu.usbmodem...`（Windows 使用 `COM...`）。

路演页面：

```sh
python monitor/roadshow_server.py
```

访问 `http://127.0.0.1:8767`。路演入口会在需要时启动带 `--roadshow-control` 的状态服务。已有只读服务时若切换按钮不可用，先停止它，再按需手动运行：

```sh
python monitor/server.py --roadshow-control
```

独立路演情境会向设备发送明确的切换指令，使用路演存档；页面的一键返回用于返回正式世界。默认只读模式与可控制模式请按场景选择。两个页面复用同一个串口读取服务。画面使用摄像头拍屏或系统已有投屏窗口，不向 ESP32 增加连续传屏负载。

可选本地模型：在本机模型服务加载 `qwen3.5-4b`，提供 `http://127.0.0.1:1234/v1/chat/completions`，然后启动：

```sh
python monitor/server.py --mind-bridge --roadshow-control
```

该模式会通过 USB 发送有边界的生活意图，不能与另一个串口程序同时打开设备。模型权重没有包含在提交包中。小智语音还需自行完成设备配网、绑定与角色配置，参考 [云端模板说明](../cloud-config/README.md)。本项目借鉴 ACE 分层思路，**未接入 NVIDIA ACE SDK**。

## 编译固件

已使用的硬件：Waveshare **ESP32-S3-Touch-AMOLED-2.16**，ESP32-S3，16 MB Flash，480×480 游戏画面，板载触摸、音频、QMI8658。其他板型不要直接套用引脚。

需要 ESP-IDF **v6.1**（上游工程最低 v6.0.1），CMake、Ninja，以及相应 ESP32-S3 工具链。ESP-IDF 安装目录独立于本包，先安装并激活它：

```sh
export IDF_PATH="/path/to/esp-idf-v6.1"
source "$IDF_PATH/export.sh"
# 回到本工程根目录
bash tools/build-firmware.sh
```

上述命令选择 `esp32-s3-touch-amoled-2.16-story-v46` 与 `zh-CN`。`vendor/xiaozhi-esp32/sdkconfig` 保留本次配置，`dependencies.lock` 保留依赖版本锁。第三方组件下载缓存与 ESP-IDF SDK 未包含，首次构建会由 ESP-IDF 组件管理器联网获取。

分区定义：`vendor/xiaozhi-esp32/partitions/v2/16m_spring_story.csv`。默认采用嵌入式游戏背景资源，图片已转换为 C/C++ 资源，不需要先重新生成美术。首次构建需要组件管理器联网下载依赖；不同 SDK 版本可能更新配置，建议使用上述版本。

本次打包和验证**不会烧录设备**。刷写现有设备前应备份，并核对硬件、分区与配套资源。仅刷 app 不能给错误分区或缺失字体/音频资源的空白设备完成首次安装。若使用 `prebuilt/`，先读该目录说明。

## 验证

需要 C++17 编译器（clang++ 或 g++）和 Python。Node.js 用于额外的前端检查。

```sh
python3 tools/test-submission.py --sanitize
```

这会运行已有的世界、经营、剧情、记忆、路演隔离、通讯与本地决策测试，以及 Python 终端测试；测试使用合成数据，不访问真实 USB 或云端模型。

原生 LVGL 界面测试另需 CMake/Ninja。须先运行一次固件构建以获取锁定的 LVGL 与设备字体依赖：

```sh
bash tools/test-touch.sh
mkdir -p evidence/ui-preview
./tests/.build/touch/spring_story_ui_test evidence/ui-preview
```

资源重新加工为可选步骤：`python -m pip install -r requirements-assets.txt`。原图、图集坐标、处理脚本与生成后的固件资源都已保留，详见素材索引。

## 第三方与素材来源

小智固件上游版权与 MIT 许可保留在 `vendor/xiaozhi-esp32/LICENSE`。小智上游许可文件保留，其他组件下载时附带各自许可，详见 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。项目美术使用用户参考图与 image_gen 制作，保留了提示词、母版和处理记录。未将第三方代码或字体重新标记为项目自有素材。

## 完整性校验

```sh
python3 tools/verify-submission.py
```

`SUBMISSION_MANIFEST.json` 保存全部交付文件的大小和 SHA-256，`SHA256SUMS` 提供通用校验清单。清单自身不循环加入自己的校验。运行后新产生的 build、data 和测试输出不属于原始交付文件。
