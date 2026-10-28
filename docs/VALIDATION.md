# 本次提交验证

验证直接针对提交副本运行，并非复用原目录的旧 build 缓存。构建时使用锁文件对应的依赖版本；下载缓存随后从交付包中移除，接收方首次构建需联网获取。

| 项目 | 结果 |
| --- | --- |
| ESP32-S3 全量固件构建 | 通过，ESP-IDF v6.1 |
| app 大小 / app 分区 | 4,261,120 / 4,653,056 字节 |
| C++ 核心测试程序 | 27 个通过，AddressSanitizer + UndefinedBehaviorSanitizer |
| 终端 Python 测试 | 23 项通过 |
| 本地决策 Python 测试 | 59 项通过 |
| 前端语法与路演回复测试 | 通过 |
| LVGL 触摸与剧情界面 | 通过，26 个剧情/档案页，2355 次设备字体字形检查 |

详情：[机器可读结果](../validation/summary.json)、[完整主机记录](../validation/host-tests.json)、[固件构建日志](../validation/firmware-build.log)、[原生界面日志](../validation/native-ui.log)。

没有连接真实设备、写入 Flash、读取玩家 NVS 或重新验收触摸/声音/温升。在线模型与实际云端对话不在本次离线验证范围。三个旧存档迁移测试依赖私有设备输入，未执行也未打包其输入。

可复跑 `python3 tools/test-submission.py --sanitize`。验证日志中的运行路径已替换为占位符；包内正式入口使用相对路径或用户自己的 SDK 环境。
