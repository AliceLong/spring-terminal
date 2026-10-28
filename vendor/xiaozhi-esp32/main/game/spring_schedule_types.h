#pragma once
#include <cstdint>
namespace spring {
enum class ScheduleBlocker : uint32_t {
    None,
    WaterFinishing,
    HarvestFinishing,
    PlayerTask,
    Construction,
    Exploration,
    Recycling,
    TreeWork,
    Emergency,
    Story,
    Resources,
    MatureCrop,
    PlayerActivity
};
struct ScheduleState {
    uint32_t approved_token = 0, approved_sequence = 0;
    ScheduleBlocker pause = ScheduleBlocker::None;
    bool waiting_short = false;
    uint32_t last_sequence = 0, last_result = 0, last_intent = 0;
    ScheduleBlocker last_cause = ScheduleBlocker::None;
};
static_assert(sizeof(ScheduleState) <= 32, "bounded transient scheduler");
inline const char* ScheduleBlockerName(ScheduleBlocker b) {
    static const char* names[] = {"none",
                                  "finishing_watering",
                                  "finishing_harvest",
                                  "player_plant_task",
                                  "construction",
                                  "exploration",
                                  "recycling",
                                  "tree_work",
                                  "emergency",
                                  "story",
                                  "resources",
                                  "mature_crop",
                                  "another_player_activity"};
    return names[unsigned(b) < 13 ? unsigned(b) : 0];
}
inline const char* ScheduleWaitText(ScheduleBlocker b, unsigned stage) {
    if (!stage) {
        static const char* text[] = {"",
                                     "浇水……收尾。",
                                     "收获……收尾。",
                                     "先做……你的安排。",
                                     "施工中……暂不能接。",
                                     "探索中……暂不能接。",
                                     "拆解中……暂不能接。",
                                     "大树……照料中。",
                                     "应急……优先。",
                                     "当前记录……先处理。",
                                     "资源不足……暂缓。",
                                     "成熟作物……先收好。",
                                     "已有安排……未完成。"};
        return text[unsigned(b) < 13 ? unsigned(b) : 0];
    }
    static const char* text[] = {"",
                                 "当前浇水正在收尾。",
                                 "当前收获正在收尾。",
                                 "正在执行你交代的植物任务。",
                                 "施工正在进行。",
                                 "探索正在进行。",
                                 "拆解正在进行。",
                                 "正在培育大树。",
                                 "先处理应急任务。",
                                 "先处理眼前这件事。",
                                 "资源不足，活动暂缓。",
                                 "先收好成熟作物。",
                                 "已有玩家任务未完成。"};
    return text[unsigned(b) < 13 ? unsigned(b) : 0];
}
}  // namespace spring
