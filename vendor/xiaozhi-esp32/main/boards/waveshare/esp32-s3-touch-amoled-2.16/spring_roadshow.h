#pragma once
#include <array>
#include <cstring>
#include "game/spring_core.h"
#include "spring_roadshow_chapters.h"

namespace spring_show {
constexpr unsigned kSceneCount = 11;
constexpr unsigned kResume = 7;
inline const char* SceneName(unsigned scene) {
    static const char* names[] = {"正式进度",
                                  "初次相遇",
                                  "共同劳动",
                                  "自主生活",
                                  "旧日档案",
                                  "返程真相",
                                  "新叶分享",
                                  "继续上次路演",
                                  "第一章：干旱现场",
                                  "第一章：优秀员工档案",
                                  "第一章：档案后的观察",
                                  "第二章：乘客座椅"};
    return scene <= kSceneCount ? names[scene] : "未知情境";
}
struct Packet {
    uint32_t boot = 0, id = 0, epoch = 0, hi = 0, lo = 0, scene = 0;
};
inline bool Same(const Packet& a, const Packet& b) {
    return a.boot == b.boot && a.id == b.id && a.epoch == b.epoch && a.hi == b.hi && a.lo == b.lo &&
           a.scene == b.scene;
}
class Line {
    std::array<char, 96> bytes{};
    unsigned length = 0;
    bool overflow = false;

public:
    static bool Parse(const char* text, Packet& out) {
        if (std::strncmp(text, "SHOW1 ", 6))
            return false;
        const char* p = text + 6;
        uint32_t words[6]{};
        for (unsigned i = 0; i < 6; ++i) {
            unsigned digits = 0;
            while (*p && *p != ' ') {
                const unsigned d = *p >= '0' && *p <= '9'   ? *p - '0'
                                   : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10
                                                            : 16;
                if (d > 15 || ++digits > 8)
                    return false;
                words[i] = (words[i] << 4) | d;
                ++p;
            }
            if (!digits || (i < 5 ? *p != ' ' : *p != 0))
                return false;
            if (i < 5)
                ++p;
        }
        if (!words[0] || !words[1] || words[5] > kSceneCount || !(words[3] | words[4]))
            return false;
        out = {words[0], words[1], words[2], words[3], words[4], words[5]};
        return true;
    }
    bool Feed(char c, Packet& out) {
        if (c == '\r')
            return false;
        if (c != '\n') {
            if (length + 1 < bytes.size())
                bytes[length++] = c;
            else
                overflow = true;
            return false;
        }
        bytes[length] = 0;
        const bool valid = !overflow && Parse(bytes.data(), out);
        length = 0;
        overflow = false;
        return valid;
    }
};
inline bool Fresh(const Packet& p, const spring::Core& c, uint32_t boot) {
    return c.s.memory && p.boot == boot && p.epoch == c.roadshow_epoch &&
           p.hi == c.s.memory->world_hi && p.lo == c.s.memory->world_lo;
}
inline const char* Blocker(const spring::Core& c, bool voice_idle, bool queue_empty) {
    if (!voice_idle)
        return "VOICE_ACTIVE";
    if (!queue_empty)
        return "COMMANDS_PENDING";
    if (c.PlantWorkActive() || c.BusyEconomy() || c.s.tree_work_ms ||
        c.work != spring::Work::Rest || c.s.work_ms || c.LifeActive())
        return "ACTIVITY_RUNNING";
    return "NONE";
}
// Fixtures establish starting facts only: no invented player requests, task results,
// or shared memories. Live actions after loading use the regular simulation.
inline bool Prepare(spring::Core& out, unsigned scene, uint32_t hi, uint32_t lo) {
    if (scene >= 8)
        return PrepareChapters(out, scene, hi, lo);
    if (!scene || scene > 6 || !(hi | lo))
        return false;
    out = {};
    out.storage_ok = true;
    out.demo = out.roadshow = true;
    out.roadshow_scene = scene;
    out.EnableEconomy(true);
    out.s.story_stage = 1;
    out.s.water = 40;
    out.s.energy = 40;
    out.s.fiber = 16;
    out.s.economy.food = 12;
    out.s.economy.parts = 4;
    out.s.economy.paid_mask = 7;
    out.s.economy.round_ms = 1000;
    out.s.tree_stage = scene >= 3 ? 1 : 0;
    if (out.s.tree_stage)
        out.s.story_stage = 2;
    if (scene >= 3)
        out.s.economy.built = (1u << 1) | (1u << 5);
    for (auto& plot : out.s.plots)
        plot.growth_ms = 200000;
    if (scene <= 2) {
        out.s.plots[0].ready = true;
        out.s.plots[0].growth_ms = 600000;
        out.s.economy.mature_yield[0] = 5;
    }
    auto& n = out.s.story;
    n.enabled = 1;
    n.language = scene == 3 ? 1 : scene == 5 ? 2 : scene == 6 ? 3 : 0;
    const unsigned target = scene == 1   ? 0
                            : scene == 2 ? 1
                            : scene == 3 ? 6
                            : scene == 4 ? 4
                            : scene == 5 ? 18
                                         : 21;
    constexpr unsigned route[] = {0, 1, 2, 3, 4, 5, 8, 9, 10, 13, 14, 16, 17, 18, 19, 20};
    for (auto node : route) {
        if (node == target || (scene == 3 && node == 8))
            break;
        n.completed |= 1u << node;
    }
    n.available = n.completed | (1u << target);
    n.pending = target;
    n.cooperation = scene >= 3 ? 1 : 0;
    n.dry_seen = scene >= 3;
    n.rain_seen = scene == 3 || scene >= 5;
    n.rest_observed = scene == 3;
    n.seed = scene >= 5;
    n.seed_choice = scene >= 5 ? 2 : 0;
    n.choices[10] = n.seed_choice;
    n.animal_discovery = scene >= 5 ? 2 : 0;
    n.new_leaf = scene == 6;
    n.head_ms = scene == 6 ? 60000 : 0;
    for (unsigned i = 0; i < spring::kStoryCount; ++i)
        if ((n.available & (1u << i)) && spring::kStorySpecs[i].archive < 7)
            n.archives |= 1u << spring::kStorySpecs[i].archive;
    // Start tracking after fixture construction: built objects become explicit baseline facts.
    out.EnableMemory(hi, lo);
    return true;
}
}  // namespace spring_show
