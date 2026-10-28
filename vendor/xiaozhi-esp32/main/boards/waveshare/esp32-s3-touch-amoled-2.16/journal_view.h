#pragma once
#include <algorithm>
#include <ctime>
#include <string>
#include "game/game_core.h"
namespace desk_gap {
inline const char* JournalItem(Item item) {
    switch (item) {
        case Item::kBottleCap:
            return "蓝色瓶盖";
        case Item::kCotton:
            return "棉花";
        case Item::kPaperclip:
            return "回形针";
        case Item::kTable:
            return "瓶盖桌";
        case Item::kBed:
            return "柔软床铺";
        case Item::kStorage:
            return "收纳架";
        case Item::kStick:
            return "木签";
        case Item::kPaper:
            return "纸屑";
        default:
            return "家园";
    }
}
inline unsigned JournalPages(const GameState& s, unsigned tab) {
    return tab == 1 ? 3 : tab == 2 ? std::max<uint32_t>(1, s.event_count) : 1;
}
inline std::string JournalText(const GameState& s, unsigned tab, unsigned page) {
    if (tab == 0) {
        const auto completed = (s.construction == ConstructionState::kCompleted) +
                               (s.bed == ConstructionState::kCompleted) +
                               (s.storage == ConstructionState::kCompleted);
        return "成长阶段 " + std::to_string(GrowthStage(s)) + "\n" +
               (GrowthStage(s) == 2   ? "柔软的小窝"
                : GrowthStage(s) == 1 ? "有了小桌子"
                                      : "简陋的住所") +
               "\n舒适 " + std::to_string(Comfort(s)) + "/45\n设施 " + std::to_string(completed) +
               "/3";
    }
    if (tab == 1) {
        const Item items[] = {Item::kBottleCap, Item::kCotton, Item::kPaperclip};
        auto item = items[page % 3];
        auto stage = ItemStage(s, item);
        const char* state = stage == ConstructionState::kCompleted    ? "已完成"
                            : stage == ConstructionState::kNotStarted ? "未投送"
                            : stage == ConstructionState::kBuilding   ? "制作中"
                            : stage == ConstructionState::kWaiting    ? "等待材料"
                            : stage == ConstructionState::kDelivered  ? "等待发现"
                                                                      : "正在搬运";
        return std::string(JournalItem(FurnitureFor(item))) + "\n" + state + "\n" +
               (item == Item::kBottleCap ? "瓶盖与木签"
                : item == Item::kCotton  ? "棉花铺设"
                                         : "整理材料") +
               "\n" +
               (item == Item::kBottleCap ? "舒适 +20"
                : item == Item::kCotton  ? "舒适 +15"
                                         : "不增加舒适");
    }
    if (!s.event_count)
        return "还没有记录\n一起慢慢发现吧";
    const auto index = s.event_count - 1 - std::min<uint32_t>(page, s.event_count - 1);
    const auto& e = s.events[index];
    std::string action;
    switch (e.type) {
        case EventType::kDelivered:
            action = "投送" + std::string(JournalItem(e.target));
            break;
        case EventType::kDiscovered:
            action = "发现" + std::string(JournalItem(e.target));
            break;
        case EventType::kCarried:
            action = "搬回" + std::string(JournalItem(e.target));
            break;
        case EventType::kMaterial:
            action = "找到" + std::string(JournalItem(e.target));
            break;
        case EventType::kBuildStarted:
            action = "制作" + std::string(JournalItem(e.target));
            break;
        case EventType::kBuildCompleted:
            action = "建成" + std::string(JournalItem(e.target));
            break;
        case EventType::kFurnitureUsed:
            action = "使用" + std::string(JournalItem(e.target));
            break;
        case EventType::kResonance:
            action = "共鸣 +" + std::to_string(e.quantity);
            break;
        case EventType::kVoice:
            action = "完成对话";
            break;
        case EventType::kShake:
            action = "检测到摇晃";
            break;
        case EventType::kKnock:
            action = "轻敲回应";
            break;
        case EventType::kOffline:
            action = "离线推进";
            break;
        default:
            action = "存档升级";
            break;
    }
    char time[24] = "时间未知";
    if (e.timestamp) {
        const std::time_t local = e.timestamp + 8 * 3600;
        std::tm tm{};
        gmtime_r(&local, &tm);
        std::strftime(time, sizeof(time), "%m-%d %H:%M", &tm);
    }
    return "记录 " + std::to_string(page + 1) + "/" + std::to_string(s.event_count) + "\n" +
           action + "\n" + (e.offline ? "离线" : "在线") + " #" + std::to_string(e.id) + "\n" +
           time;
}
}  // namespace desk_gap
