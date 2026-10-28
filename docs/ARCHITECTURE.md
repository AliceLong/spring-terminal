# 架构与代码入口

| 职责 | 主要实现 |
| --- | --- |
| 事实权威与规则提交 | `vendor/xiaozhi-esp32/main/game/spring_core.h` 及相邻实现 |
| 资源、作物、设施与动物 | `spring_economy_impl.h`、`spring_economy_types.h` |
| 条件解锁与剧情事实 | `spring_story_impl.h`、`spring_story_types.h` |
| 自主生活与行动调度 | `spring_life_impl.h`、`spring_schedule_impl.h` |
| 真实经历与持久事实 | `spring_memory*.h`、`spring_story_impl.h` 中的保存实现 |
| 角色、关注、许可与分享 | `spring_character.h`、`spring_agency.h`、`spring_social.h` |
| 设备与语音工具入口 | 板级 `game_tools.cc`、`day1_probe.cc` |
| 画面与实体动作 | 板级 `spring_art*.cc`、`spring_gardener.h`、`spring_story_ui.inc` |
| 独立路演存档 | 板级 `spring_roadshow.h/.inc`，`monitor/roadshow_control.py` |
| 本地决策与串口桥接 | `robot-mind/director.py`、`observation.py`、`device_bridge.py` |

表内未写目录的文件均在 `vendor/xiaozhi-esp32/main/game/`，以包内实际文件为准。

输入来自同一份世界状态与已解锁事实。小智负责语音交流和玩家邀请，本地低频决策负责生活选择，剧情执行器按确定性条件推进。候选动作需要检查当前状态与许可，由 FSM 执行，实际到达/完成后回写世界和经历。提出、批准、执行、完成分别记录。

角色语言阶段不等于做事能力，也不等于剧情章节。未解锁历史不能靠自由聊天改写。费用和建造由本地规则提交，模型不能直接改库存。独处活动不会自动标成与玩家共同参与。

语音会话会暂停自主生活；玩家邀请动作与语音的并行只按已有明确入口执行。长期记忆目前包含有边界的结构化事件与事实，不等于自由聊天自动生成情感回忆。

ACE 的感知、认知、行动、表现分工提供架构参考，本包没有 ACE SDK 依赖。
