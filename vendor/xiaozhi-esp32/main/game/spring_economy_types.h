#pragma once
// Included inside namespace spring, after Plot. Pure simulation data only.
enum class Facility : uint32_t { Tank, Rain, Solar, Battery, Nursery, Bench, Care, Nest };
enum class Goal : uint32_t { Survival, Tree, Construction, Animals };
enum class ProposalKind : uint32_t { None, Build, Plant, Goal, Boost, Journey, Salvage, Tree };
enum class Journey : uint32_t { None, Salvage, Supply, Rain, Bird, Mouse, Food };
struct FacilitySpec {
    const char* id;
    const char* name;
    uint32_t fiber, parts, water, energy, ms;
    const char* benefit;
    const char* upkeep;
};
constexpr std::array<FacilitySpec, 8> kFacilities{
    {{"tank", "储水罐", 9, 1, 4, 0, 120000, "水容量增加40", "无持续消耗"},
     {"rain", "集雨器", 6, 1, 0, 0, 120000, "每10分钟水+1；降雨额外+6", "无持续消耗"},
     {"solar", "车顶集能板", 12, 2, 0, 0, 180000, "每10分钟能源+1", "无持续消耗"},
     {"battery", "蓄能箱", 9, 1, 0, 4, 120000, "能源容量增加40", "无持续消耗"},
     {"nursery", "种植架", 9, 1, 4, 0, 120000, "第4位默认种旅粮豆", "每轮水2能源1，可改种"},
     {"bench", "回收工作台", 9, 1, 0, 0, 180000, "拆解1件待回收物得零件2", "每次能源2、工作1分钟"},
     {"care", "照料装置", 12, 2, 0, 0, 180000, "指定苔、藤或豆产物+1", "该作物每轮能源1；可关闭"},
     {"nest", "动物小窝", 6, 1, 4, 0, 120000, "安置寻雨雀和拾荒鼠", "入住后每30分钟扣照料资源"}}};
constexpr std::array<const char*, 4> kGoals{"维持供给", "优先养树", "优先建设", "准备救助"};
constexpr std::array<const char*, 7> kJourneys{"无旅途事件",  "发现废弃设备", "发现补给",
                                               "途经降雨区",  "寻雨雀求助",   "拾荒鼠求助",
                                               "发现食物补给"};
constexpr std::array<const char*, 7> kJourneyOpenings{
    "暂时没有需要你决定的旅途事件。",         "前面有台废弃设备。愿意让我停留探索吗？",
    "我发现了一份补给。要把它带上车吗？",     "雨来了。我们要停留收集一些水吗？",
    "窗边有只疲惫的小鸟。我们能接它上车吗？", "废墟里有只小老鼠。要留一点食物接它上车吗？",
    "发现了一些食物。要收集上车吗？"};
// The accepted journey is kept in journey_task after the incoming call is cleared.
// These labels describe the actual task, not an imagined repair or a new story node.
constexpr std::array<const char*, 7> kJourneyTaskNames{
    "没有旅途任务", "检查废弃设备", "搬运补给", "收集雨水", "照看寻雨雀", "照看拾荒鼠", "收集食物"};
constexpr std::array<const char*, 7> kJourneyTaskResults{
    "",         "带回待回收物，仍需在工作台拆解", "带回水4", "收集水6", "寻雨雀入住", "拾荒鼠入住",
    "带回食物2"};
struct Proposal {
    uint32_t kind = 0, target = 0, value = 0, token = 0;
};
struct DecisionEntry {
    uint32_t id = 0, second = 0;
    Event type = Event::Wake;
    uint32_t target = 0, amount = 0;
};
struct EconomyState {
    bool enabled = false;
    uint32_t region = 0, region_ms = 0, emergency_work = 0, emergency_cooldown = 0;
    uint32_t pending_journey = 0, recycle_yield = 0, bird_visit = 0, mouse_help_explorations = 0;
    uint32_t warning_epoch = 0, warning_mask = 0, warning_reason = 0;
    bool round_worked = false;
    std::array<uint32_t, 4> mature_yield{};
    uint32_t food = 0, parts = 2, built = 0, construction = 255, construction_ms = 0, boost = 255,
             paid_mask = 0, boost_paid = 0;
    Plot fourth{Plant::Food, Plant::Food};
    uint32_t animals = 0, animal_help = 0, scrap = 0, recycle_ms = 0, journey_work = 0,
             journey_clock = 0, round_ms = 0, emergency_ms = 0;
    uint32_t journey_task = 0, recommendation_stamp = 0;
    uint32_t journey = 0, journey_id = 0, journey_seq = 0, journey_read = 0, scavenges = 0,
             proposal_seq = 0, discovered_plants = 7, discovered_animals = 0;
    std::array<uint32_t, 4> priorities{0, 1, 2, 3};
    std::array<uint32_t, 2> animal_elapsed{0, 0};
    Proposal proposal{};
    uint32_t decision_count = 0;
    std::array<DecisionEntry, 12> decisions{};
};
constexpr std::array<const char*, 5> kRegions{"晴朗平原", "干旱荒原", "雨中旧城", "废弃隧道",
                                              "繁茂森林"};
constexpr unsigned kRegionYields[5][4] = {
    {5, 6, 3, 2}, {6, 4, 2, 2}, {4, 8, 3, 2}, {4, 4, 2, 2}, {5, 7, 4, 3}};
inline uint32_t TreeWater(unsigned stage) { return stage ? 6 : 4; }
inline uint32_t TreeFiber(unsigned stage) { return stage ? 6 : 3; }
inline uint32_t TreeDuration(unsigned stage) { return stage ? 120000 : 60000; }
using LegacyFullSave = std::array<uint32_t, 256>;
using FullSave = std::array<uint32_t, 384>;
