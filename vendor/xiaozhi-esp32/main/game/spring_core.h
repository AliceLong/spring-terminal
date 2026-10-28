#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include "spring_agency_types.h"
#include "spring_attention_policy.h"
#include "spring_chapter_types.h"
#include "spring_life_types.h"
#include "spring_memory_types.h"
#include "spring_plant_action_types.h"
#include "spring_schedule_types.h"
#include "spring_story_types.h"

// Pure, deterministic world simulation. No LVGL, ESP, audio or network dependencies.
namespace spring {
enum class Plant : uint32_t { Light, Moss, Vine, Food };
enum class Event : uint32_t {
    Wake,
    Planted,
    Matured,
    Harvest,
    Irrigated,
    TreeStarted,
    TreeGrown,
    Drought,
    Reply,
    FacilityStarted,
    FacilityBuilt,
    JourneyFound,
    JourneyResolved,
    PlanChanged,
    ProposalCreated,
    AnimalFed,
    Recycled,
    Shortage,
    JourneyCompleted,
    RegionChanged,
    BirdVisited,
    EmergencyStarted,
    EmergencyDone
};
enum class Work : uint32_t { Rest, Water, Harvest, Build };
constexpr uint32_t kCapacity = 99;
// Includes travel to the plant, sustained watering, and putting the can away.
constexpr uint32_t kWaterWorkMs = 10000;
constexpr std::array<uint32_t, 4> kGrowMs{180000, 240000, 300000, 600000};
constexpr std::array<const char*, 4> kPlantIds{"tracklight", "dewmoss", "patchvine", "travelbean"};
constexpr std::array<const char*, 4> kPlantNames{"轨光花", "雨窝苔", "缝甲藤", "旅粮豆"};
inline const char* EventName(Event e) {
    static const char* names[] = {"ROBOT_AWAKENED",
                                  "NEXT_PLANT_SELECTED",
                                  "PLANT_MATURED",
                                  "HARVESTED",
                                  "IRRIGATED",
                                  "TREE_STARTED",
                                  "TREE_GROWN",
                                  "DROUGHT_APPROACHING",
                                  "COMMUNICATION_ACCEPTED",
                                  "FACILITY_STARTED",
                                  "FACILITY_BUILT",
                                  "JOURNEY_FOUND",
                                  "JOURNEY_RESOLVED",
                                  "PLAN_CHANGED",
                                  "PROPOSAL_CREATED",
                                  "ANIMAL_FED",
                                  "RECYCLED",
                                  "SHORTAGE",
                                  "JOURNEY_COMPLETED",
                                  "REGION_CHANGED",
                                  "BIRD_VISITED",
                                  "EMERGENCY_STARTED",
                                  "EMERGENCY_DONE"};
    return names[static_cast<unsigned>(e) < 23 ? static_cast<unsigned>(e) : 0];
}
struct Plot {
    Plant plant = Plant::Light, next = Plant::Light;
    uint32_t growth_ms = 0, mature_ms = 0, cycle = 1;
    bool ready = false;
};
#include "spring_economy_types.h"
struct Record {
    uint32_t id = 0, second = 0;
    Event type = Event::Wake;
    uint32_t target = 0, amount = 0;
};
struct State {
    // Transient body job; never replayed from a save. Evidence uses existing memory records.
    PlantAction plant_action;
    ScheduleState schedule;
    EconomyState economy;
    StoryState story;
    ChapterState chapters;
    LifeState life;
    std::shared_ptr<MemoryData> memory;
    uint32_t water = 8, energy = 8, fiber = 0;
    std::array<Plot, 3> plots{
        {{Plant::Light, Plant::Light}, {Plant::Moss, Plant::Moss}, {Plant::Vine, Plant::Vine}}};
    uint32_t tree_stage = 0, tree_work_ms = 0, story_stage = 0;
    uint64_t game_ms = 0;
    uint32_t base_ms = 0, work_ms = 0;
    uint32_t notices = 0, announced = 0, accepted = 0;
    bool sound_muted = false;
    uint32_t event_count = 0, next_event = 1;
    std::array<Record, 12> events{};
    uint32_t revision = 1;
    int64_t wall_epoch = 0;
    uint32_t unsynced_ms = 0;
};
struct Snapshot : State {
    bool storage_ok = false, clock_ok = false, demo = false, motion_ok = false;
    bool roadshow = false;
    uint32_t roadshow_scene = 0, roadshow_epoch = 0;
    Work work = Work::Rest;
    int target = 0;
    float dew_path = 0;
    int dew_slot = -1;
    uint32_t offline_seconds = 0;
};
using Save = std::array<uint32_t, 128>;
struct StorySave {
    uint32_t magic = 0x53543636, version = 1;
    FullSave core{};
    StoryState story{};
    MemoryData memory{};
    uint32_t checksum = 0;
};
static_assert(sizeof(StorySave) % 4 == 0, "story save word alignment");
struct StoryCheckpoint {
    uint32_t magic = 0x53434b36, version = 1, generation = 0, last_id = 0, world_hi = 0,
             world_lo = 0;
    FullSave core{};
    StoryState story{};
    uint32_t checksum = 0;
};

inline const char* StoryNodeOpening(const Snapshot& s, unsigned id) {
    if (id >= kStoryCount)
        return "暂时没有待处理的剧情。";
    if (id == 2) {
        if (s.economy.region != 1)
            return "我们经过了一段干旱。我想和你回看供水安排。";
        if (s.water < 4)
            return "水不太够。我会先按现有应急安排处理，新的安排想和你商量。";
        if (s.water >= 20)
            return "这一段比较干。目前储备够，我想提前看看安排。";
    }
    if (id == 4 && (s.story.archives_read & 2u)) {
        if (s.chapters.requested_cares || s.chapters.requested_harvests ||
            s.chapters.manual_harvests)
            return "刚才……照料。以前……清退。评价，优秀？";
        return "现在，维护。以前……清退。都是……工作？";
    }
    if (id == 6) {
        if (ChapterHas(s.chapters, BenchRestCompleted))
            return "原来……从这里看，是这样。";
        if (ChapterHas(s.chapters, BeyondWorkObserved))
            return "乘客座椅。工作人员……可以用？";
    }
    if (id == 5 && s.economy.region != 2)
        return "记录显示，我们经过了雨中旧城。那段干旱已经过去了。";
    if (id == 7 && s.story.robot_named)
        return "你之前叫我的那个名字，不是工作编号，对吗？";
    if (id == 13 && !s.story.flower)
        return "G-17的记录里，有17次继续观察。现在，我也想继续观察。";
    if (id == 14 && s.economy.animals)
        return "小窝已经有动物。我想和你看看照料的安排。";
    return kStorySpecs[id].opening;
}
inline const char* StoryNodeDetail(const Snapshot& s, unsigned id) {
    if (id >= kStoryCount)
        return "暂时没有待处理片段。";
    if (id == unsigned(StoryNode::Rest) && s.chapters.version) {
        if (ChapterHas(s.chapters, BenchRestCompleted))
            return "它曾供乘客看风景。\n这次，我坐上去了。\n之后休息，可以使用这张椅子。";
        if (ChapterHas(s.chapters, BeyondWorkObserved))
            return "旧乘客观光座椅。\n工作人员使用……需要许可。\n你可以邀请01-"
                   "99坐下，也可以稍后再说。";
        return "现在可以在地面短暂休息。\n乘客座椅的故事，还没有发生。";
    }
    return kStorySpecs[id].detail;
}
inline const char* StoryOpening(const Snapshot& s) {
    return StoryNodeOpening(s, StoryCurrentNode(s.story));
}

class Core {
public:
    State s;
    bool storage_ok = false, clock_ok = false, demo = false;
    // Session metadata is deliberately excluded from every persistent game format.
    bool roadshow = false;
    uint32_t roadshow_scene = 0, roadshow_epoch = 0;
    uint32_t offline_seconds = 0;
    float dew_path = 0;
    int dew_slot = -1;
    bool tilt_armed = false;
    Work work = Work::Rest;
    int target = 0;
    uint32_t real_shake_cooldown = 0;
    bool life_recovered = false;
    void Add(Event e, uint32_t item = 0, uint32_t amount = 0,
             MemoryActor actor = MemoryActor::Unknown) {
        if (s.event_count == s.events.size()) {
            std::move(s.events.begin() + 1, s.events.end(), s.events.begin());
            --s.event_count;
        }
        s.events[s.event_count++] = {s.next_event++, static_cast<uint32_t>(s.game_ms / 1000), e,
                                     item, amount};
        if (s.economy.enabled && e != Event::Harvest && e != Event::Matured &&
            e != Event::Irrigated && e != Event::AnimalFed && e != Event::Shortage) {
            auto& economy = s.economy;
            if (economy.decision_count == economy.decisions.size()) {
                std::move(economy.decisions.begin() + 1, economy.decisions.end(),
                          economy.decisions.begin());
                --economy.decision_count;
            }
            const auto& r = s.events[s.event_count - 1];
            economy.decisions[economy.decision_count++] = {r.id, r.second, r.type, r.target,
                                                           r.amount};
        }
        ++s.revision;
        MemoryWorld(e, item, amount, s.next_event - 1, actor);
        StoryWorldEvent(e, item, actor);
    }
    // Fill caller-owned storage so UI refreshes need no large temporary on the stack.
    void Get(Snapshot& out) const {
        static_cast<State&>(out) = s;
        out.storage_ok = storage_ok;
        out.clock_ok = clock_ok;
        out.demo = demo;
        out.roadshow = roadshow;
        out.roadshow_scene = roadshow_scene;
        out.roadshow_epoch = roadshow_epoch;
        out.work = work;
        out.target = target;
        out.dew_path = dew_path;
        out.dew_slot = dew_slot;
        out.offline_seconds = offline_seconds;
        out.motion_ok = false;
    }
    Snapshot Get() const {
        Snapshot out;
        Get(out);
        return out;
    }
    bool Wake() {
        if (!storage_ok || s.story_stage)
            return false;
        s.story_stage = 1;
        s.notices |= 1;
        // A remaining drop offers a physical cooperation immediately after boot.
        auto& dew = s.plots[1];
        dew.growth_ms = s.economy.enabled ? 600000u : kGrowMs[1];
        if (s.economy.enabled)
            s.economy.mature_yield[1] = 6;
        dew.ready = true;
        if (s.story.enabled) {
            s.plots[0].ready = true;
            s.plots[0].growth_ms = 600000;
            s.economy.mature_yield[0] = 5;
        }
        Add(Event::Wake, 0, 0, MemoryActor::Player);
        return true;
    }
    unsigned PlotCount() const { return s.economy.enabled && HasFacility(4) ? 4 : 3; }
    Plot& PlotAt(unsigned i) { return i < 3 ? s.plots[i] : s.economy.fourth; }
    const Plot& PlotAt(unsigned i) const { return i < 3 ? s.plots[i] : s.economy.fourth; }
    bool Select(unsigned slot, Plant p) {
        if (!storage_ok || !s.story_stage || slot >= PlotCount() ||
            static_cast<unsigned>(p) >= (s.economy.enabled ? 4u : 3u))
            return false;
        if (s.memory && !s.memory->jobs[4 + slot].chain) {
            const Proposal direct{2, slot, static_cast<uint32_t>(p), 0};
            const auto job = MemoryApproved(direct);
            MemoryDecisionResult(direct, job, false);
        }
        PlotAt(slot).next = p;
        Add(Event::Planted, slot, static_cast<uint32_t>(p));
        return true;
    }
    void Notice(uint32_t bit) { s.notices |= bit & ~s.accepted; }
    bool Accept(unsigned bit) {
        if (!storage_ok || bit >= 6 || !(s.notices & (1u << bit)))
            return false;
        s.notices &= ~(1u << bit);
        if (bit == 3 && s.economy.enabled) {
            // Supply alerts are acknowledged per region, not permanently suppressed.
        } else if (bit == 4)
            s.economy.journey_read = s.economy.journey_id;
        else if (bit == 5) {
        } else
            s.accepted |= 1u << bit;
        Add(Event::Reply, bit, 0, MemoryActor::Player);
        return true;
    }
    bool Announce(unsigned bit) {
        if (!storage_ok || bit >= 6 || !(s.notices & (1u << bit)) || (s.announced & (1u << bit)))
            return false;
        s.announced |= 1u << bit;
        ++s.revision;
        return true;
    }
    bool BuildTree() {
        if (!storage_ok || !s.story_stage || s.tree_stage >= 2 || s.tree_work_ms ||
            (s.economy.enabled && BusyEconomy()) ||
            s.water < (s.economy.enabled ? TreeWater(s.tree_stage) : 6u) ||
            s.fiber < (s.economy.enabled ? TreeFiber(s.tree_stage) : 4u))
            return false;
        if (s.memory && !s.memory->jobs[1].chain)
            MemoryApproved(Proposal{7, 0, 0, 0});
        s.water -= s.economy.enabled ? TreeWater(s.tree_stage) : 6u;
        s.fiber -= s.economy.enabled ? TreeFiber(s.tree_stage) : 4u;
        s.tree_work_ms = 1;
        Add(Event::TreeStarted);
        return true;
    }
    bool Harvest(unsigned slot, bool irrigate = false, MemoryActor actor = MemoryActor::Robot) {
        if (s.economy.enabled)
            return EconomyHarvest(slot, irrigate, actor);
        if (!storage_ok || slot >= 3 || !s.plots[slot].ready)
            return false;
        auto& p = s.plots[slot];
        const auto kind = p.plant;
        uint32_t amount = kind == Plant::Vine ? 2 : 4;
        auto* pool = kind == Plant::Light ? &s.energy : kind == Plant::Moss ? &s.water : &s.fiber;
        const auto before = *pool;
        if (irrigate && kind == Plant::Moss) {
            for (auto& other : s.plots)
                if (!other.ready)
                    other.growth_ms = std::min(kGrowMs[static_cast<unsigned>(other.plant)],
                                               other.growth_ms + 30000);
            Add(Event::Irrigated, slot, 30);
        } else {
            *pool = std::min(kCapacity, *pool + amount);
            Add(Event::Harvest, slot, *pool - before);
        }
        // Harvest grants once per cycle. Animation is a view of this committed state.
        if (kind != Plant::Moss && s.water)
            --s.water;
        if (kind != Plant::Light && s.energy)
            --s.energy;
        p.plant = p.next;
        p.ready = false;
        p.growth_ms = p.mature_ms = 0;
        ++p.cycle;
        if (dew_slot == static_cast<int>(slot)) {
            dew_slot = -1;
            dew_path = 0;
            tilt_armed = false;
        }
        work = Work::Harvest;
        target = static_cast<int>(slot);
        s.work_ms = 2500;
        return true;
    }
    unsigned Shake() {
        if (!s.story_stage) {
            Wake();
            return 0;
        }
        if (real_shake_cooldown)
            return 0;
        real_shake_cooldown = 1200;
        unsigned count = 0;
        for (unsigned i = 0; i < PlotCount(); ++i)
            if (PlotAt(i).plant != Plant::Moss && Harvest(i, false, MemoryActor::Player))
                ++count;
        return count;
    }
    // Gravity-filtered tilt; dynamic motions cancel the path rather than awarding.
    bool Tilt(float angle, bool stable, uint32_t real_ms) {
        if (!storage_ok || !s.story_stage)
            return false;
        if (!std::isfinite(angle) || std::abs(angle) > 55) {
            dew_path = 0;
            dew_slot = -1;
            tilt_armed = false;
            return false;
        }
        if (!stable) {
            // Moving from neutral into a tilt is expected. Pause/cancel progress,
            // but retain neutral arming so a settled tilt can resume the gesture.
            dew_path = 0;
            return false;
        }
        int slot = -1;
        for (unsigned i = 0; i < PlotCount(); ++i)
            if (PlotAt(i).ready && PlotAt(i).plant == Plant::Moss) {
                slot = i;
                break;
            }
        if (slot < 0) {
            dew_path = 0;
            dew_slot = -1;
            tilt_armed = false;
            return false;
        }
        if (dew_slot != slot) {
            dew_path = 0;
            dew_slot = slot;
            tilt_armed = false;
        }
        if (std::abs(angle) < 8)
            tilt_armed = true;
        if (!tilt_armed)
            return false;
        if (std::abs(angle) >= 11)
            dew_path = std::clamp(
                dew_path + (angle > 0 ? 1.f : -1.f) * std::min<uint32_t>(real_ms, 100) / 1800.f,
                -1.f, 1.f);
        if (std::abs(dew_path) >= .999f)
            return Harvest(slot, dew_path > 0, MemoryActor::Player);
        return false;
    }
    void Step(uint32_t real_ms) {
        real_shake_cooldown -= std::min(real_shake_cooldown, real_ms);
        if (!storage_ok || !s.story_stage)
            return;
        Advance(real_ms * (demo && !roadshow ? 10u : 1u));
        TickStory(real_ms);
        s.unsynced_ms =
            static_cast<uint32_t>(std::min<uint64_t>(43200000, uint64_t(s.unsynced_ms) + real_ms));
    }
    void Advance(uint32_t ms) {
        if (s.economy.enabled) {
            AdvanceEconomy(ms);
            return;
        }
        // Bounded one-second simulation also used for offline settlement.
        ms = std::min<uint32_t>(ms, 43200000);
        while (ms) {
            const auto dt = std::min<uint32_t>(ms, 1000);
            ms -= dt;
            const auto previous_ms = s.game_ms;
            s.game_ms += dt;
            s.base_ms += dt;
            if (s.game_ms / 45000 != previous_ms / 45000 && s.energy)
                --s.energy;
            if (s.game_ms / 60000 != previous_ms / 60000 && s.water)
                --s.water;
            if (s.base_ms >= 90000) {
                s.base_ms -= 90000;
                s.water = std::min(kCapacity, s.water + 1);
                s.energy = std::min(kCapacity, s.energy + 1);
            }
            if (s.work_ms) {
                s.work_ms -= std::min(s.work_ms, dt);
                if (!s.work_ms)
                    work = Work::Rest;
            }
            const bool dry = s.game_ms >= 600000 && s.game_ms < 900000;
            for (unsigned i = 0; i < 3; ++i) {
                auto& p = s.plots[i];
                if (p.ready) {
                    p.mature_ms += dt;
                    if (p.mature_ms >= 45000)
                        Harvest(i);
                } else {
                    const uint32_t divisor = (!s.water ? 4 : dry ? 2 : 1) * (!s.energy ? 2 : 1);
                    p.growth_ms = std::min(kGrowMs[static_cast<unsigned>(p.plant)],
                                           p.growth_ms + dt / divisor);
                    if (p.growth_ms == kGrowMs[static_cast<unsigned>(p.plant)]) {
                        p.ready = true;
                        p.mature_ms = 0;
                        Notice(2);
                        Add(Event::Matured, i, static_cast<uint32_t>(p.plant));
                    }
                }
            }
            if (s.tree_work_ms) {
                work = Work::Build;
                s.tree_work_ms += dt / (s.energy ? 1 : 2);
                if (s.tree_work_ms >= 60000) {
                    s.tree_work_ms = 0;
                    ++s.tree_stage;
                    s.story_stage = 2;
                    Notice(4);
                    Add(Event::TreeGrown, 0, s.tree_stage);
                    work = Work::Rest;
                    s.work_ms = 0;
                }
            } else if (!s.work_ms && s.game_ms % 20000 < dt) {
                work = Work::Water;
                target = (s.game_ms / 20000) % 3;
                s.work_ms = kWaterWorkMs;
            }
            if (s.game_ms >= 540000 && !(s.accepted & 8) && !(s.notices & 8)) {
                Notice(8);
                Add(Event::Drought);
            }
            ++s.revision;
        }
    }
    void Sync(int64_t epoch) {
        if (epoch < 1700000000 || epoch > 4102444800LL)
            return;
        if (!clock_ok && s.wall_epoch && epoch >= s.wall_epoch) {
            const auto seconds =
                std::max<int64_t>(0, epoch - s.wall_epoch - (s.unsynced_ms + 999) / 1000);
            offline_seconds = static_cast<uint32_t>(std::min<int64_t>(43200, seconds));
            if (s.story_stage) {
                Advance(offline_seconds * 1000);
                if (s.story.enabled && s.story.flower && s.story.flower < 4) {
                    s.story.flower_ms =
                        std::min<uint32_t>(360000u, s.story.flower_ms + offline_seconds * 1000);
                    s.story.flower = 1 + std::min<uint32_t>(3u, s.story.flower_ms / 120000u);
                }
            }
        }
        clock_ok = true;
        s.wall_epoch = epoch;
        s.unsynced_ms = 0;
    }
    Save Pack() const {
        Save words{};
        size_t n = 0;
        auto put = [&](uint32_t x) { words[n++] = x; };
        put(0x53505247);
        put(1);
        put(s.water);
        put(s.energy);
        put(s.fiber);
        for (const auto& p : s.plots) {
            put(static_cast<uint32_t>(p.plant));
            put(static_cast<uint32_t>(p.next));
            put(p.growth_ms);
            put(p.mature_ms);
            put(p.cycle);
            put(p.ready);
        }
        put(s.tree_stage);
        put(s.tree_work_ms);
        put(s.story_stage);
        put(static_cast<uint32_t>(s.game_ms));
        put(static_cast<uint32_t>(s.game_ms >> 32));
        put(s.base_ms);
        put(s.work_ms);
        put(s.notices);
        put(s.announced);
        put(s.accepted);
        put(s.event_count);
        put(s.next_event);
        put(s.revision);
        put(static_cast<uint32_t>(s.wall_epoch));
        put(static_cast<uint32_t>(static_cast<uint64_t>(s.wall_epoch) >> 32));
        put(s.unsynced_ms);
        put(s.sound_muted);
        for (const auto& e : s.events) {
            put(e.id);
            put(e.second);
            put(static_cast<uint32_t>(e.type));
            put(e.target);
            put(e.amount);
        }
        words.back() = Checksum(words);
        return words;
    }
    bool Restore(const Save& words, bool extended = false) {
        if (words[0] != 0x53505247 || words[1] != 1 || words.back() != Checksum(words))
            return false;
        State next;
        size_t n = 2;
        auto get = [&]() { return words[n++]; };
        next.water = get();
        next.energy = get();
        next.fiber = get();
        if (next.water > (extended ? 100u : 99u) || next.energy > (extended ? 100u : 99u) ||
            next.fiber > 99)
            return false;
        for (auto& p : next.plots) {
            p.plant = static_cast<Plant>(get());
            p.next = static_cast<Plant>(get());
            p.growth_ms = get();
            p.mature_ms = get();
            p.cycle = get();
            const auto ready = get();
            if (static_cast<unsigned>(p.plant) >= 4 || static_cast<unsigned>(p.next) >= 4 ||
                ready > 1 || !p.cycle)
                return false;
            p.ready = ready;
            if (p.growth_ms > (extended ? 600000u : kGrowMs[static_cast<unsigned>(p.plant)]) ||
                p.mature_ms > 45000 ||
                (p.ready && p.growth_ms != kGrowMs[static_cast<unsigned>(p.plant)] &&
                 (!extended || p.growth_ms != 600000)) ||
                (!p.ready && p.mature_ms))
                return false;
        }
        next.tree_stage = get();
        next.tree_work_ms = get();
        next.story_stage = get();
        next.game_ms = get();
        next.game_ms |= uint64_t(get()) << 32;
        next.base_ms = get();
        next.work_ms = get();
        next.notices = get();
        next.announced = get();
        next.accepted = get();
        next.event_count = get();
        next.next_event = get();
        next.revision = get();
        uint64_t wall = get();
        wall |= uint64_t(get()) << 32;
        next.wall_epoch = static_cast<int64_t>(wall);
        next.unsynced_ms = get();
        const auto sound = get();
        if (sound > 1)
            return false;
        next.sound_muted = sound;
        if (next.tree_stage > 2 ||
            next.tree_work_ms >= (extended ? TreeDuration(next.tree_stage) : 60000u) ||
            next.story_stage > 2 || next.base_ms >= 90000 || next.work_ms > kWaterWorkMs ||
            next.notices > 63 || next.announced > 63 || next.accepted > 15 ||
            (next.notices & next.accepted) || next.event_count > 12 || !next.next_event ||
            !next.revision || next.unsynced_ms > 43200000 || next.wall_epoch < 0 ||
            next.wall_epoch > 4102444800LL || (next.tree_stage >= 2 && next.tree_work_ms) ||
            (next.tree_stage && next.story_stage != 2))
            return false;
        uint32_t previous = 0;
        for (unsigned i = 0; i < next.events.size(); ++i) {
            auto& e = next.events[i];
            e.id = get();
            e.second = get();
            e.type = static_cast<Event>(get());
            e.target = get();
            e.amount = get();
            if (i < next.event_count &&
                (e.id <= previous || e.id >= next.next_event ||
                 static_cast<unsigned>(e.type) >= 23 || e.second > next.game_ms / 1000))
                return false;
            previous = e.id;
        }
        s = next;
        tilt_armed = false;
        dew_path = 0;
        dew_slot = -1;
        work = Work::Rest;
        real_shake_cooldown = 0;
        clock_ok = false;
        return true;
    }
#include "spring_chapter_impl.h"
#include "spring_economy_impl.h"
#include "spring_life_impl.h"
#include "spring_memory_impl.h"
#include "spring_plant_action_impl.h"
#include "spring_schedule_impl.h"
#include "spring_story_impl.h"
    static uint32_t Checksum(const Save& w) {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i + 1 < w.size(); ++i)
            for (unsigned shift = 0; shift < 32; shift += 8) {
                hash ^= (w[i] >> shift) & 255;
                hash *= 16777619;
            }
        return hash;
    }
};
inline const char* Opening(unsigned bit, const Snapshot& s) {
    if (bit == 6 && s.story.enabled)
        return StoryOpening(s);
    if (bit == 3 && s.economy.enabled) {
        if (s.economy.warning_reason == 3)
            return "供水和能源都不足。我先应急取水，原来的工作进度会保留。";
        if (s.economy.warning_reason == 2)
            return "能源用完了，施工先暂停。我会优先照料能源植物；没有的话，需要你批准改种。";
        return "前面的地区供水会减少，照现在的种植，储水可能下降。我们一起看看安排？";
    }
    if (bit == 2 && s.economy.enabled && s.economy.bird_visit && s.tree_stage == 1)
        return "树长大了，有只小鸟来看看。它还没入住，等树冠繁茂，我们可以给它准备住所。";
    if (bit == 5)
        return "按照你之前的安排，我准备好了一份方案。要一起看看吗？";
    if (bit == 4 && s.economy.journey < kJourneyOpenings.size())
        return kJourneyOpenings[s.economy.journey];
    switch (bit) {
        case 0:
            return s.story_stage >= 2 ? "你回来了。谢谢你，让这里重新有了生命。"
                                      : "信号……是你？一起……维持生命。";
        case 1:
            return s.story_stage >= 2 ? "刚才有植物成熟了。你愿意帮我收获吗？"
                                      : "植物……成熟。你……帮忙？";
        case 2:
            return "新叶长出来了。你看见了吗？我想在这里，歇一会儿。";
        default:
            return s.game_ms >= 900000   ? "我们已经穿过干旱区了，植物仍然好好的。"
                   : s.game_ms >= 600000 ? "已经进入干旱区。植物会长得慢一点，我们还有回收的水。"
                                         : "前面……干旱区。水……留一些。";
    }
}
}  // namespace spring
