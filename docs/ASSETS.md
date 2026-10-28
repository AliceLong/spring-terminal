# 素材索引

| 路径 | 用途 |
| --- | --- |
| `assets/art-v1/masters/` | 车厢、窗景、植物、角色与图标母版 |
| `assets/art-v1/economy/` | 设施、动物、独立设备架与车厢布局素材 |
| `assets/art-v1/robot-hover-v1/` | 悬浮园丁基础动作母版与统一帧 |
| `assets/art-v1/robot-life-v2/` | 照料、观察、收获、休息等生活动作 |
| `assets/art-v1/passenger-seat-v2/` | 乘客座椅及休息相关素材 |
| `assets/art-v1/window-regions-v1/` | 沿途地区窗景与近远景分层 |
| `assets/art-v1/story-v46/` | 纪念花、剧情对象及预览 |
| `assets/art-v1/device/` | LVGL 像素转换产物及清单 |
| `assets/harvest-v56/` | 收获接取实验图标，非正式数值改动 |
| 板级 `spring_art_data.cc` 等 | 固件实际编译的游戏资源数组 |

以固件源文件实际引用为运行时权威，不把每一个旧 PNG 变体都宣称为当前上屏素材。保留部分历史母版/注册帧是为了追溯和重新加工，不将旧画面作为最新版本验证。

图集生成提示词、尺寸和锚点在各目录的 `manifest.json`、`prompts.json` 或 `generation-prompts.md`。美术使用用户参考图与 image_gen 生成，处理脚本完成裁切、锚点对齐与 LVGL 编码。固件背景默认嵌入，实验性外置方案不作为本提交默认配置。

该包已含游戏美术与转换结果。无需外部生成服务即可查看既有美术或编译现有 C/C++ 资源；重新生成图像本身需要另行调用生成工具。

未包含旧《工位缝隙日记》整套素材、个人设备照片、量产概念图或重复的素材 ZIP。
