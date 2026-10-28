#pragma once
#include <array>
#include <cstdint>
namespace spring {
enum class StoryVerb : uint32_t {
    Accept,
    Complete,
    Pause,
    Archive,
    OfferFlower,
    ApproveFlower,
    OfferName,
    ConfirmName,
    Observe,
    Announce,
    Inspect,
    CancelName,
    ArchiveSeen,
    ShowLife,
    Presented,
    InviteSeat,
    EndCommunication
};
enum class StoryNode : uint32_t {
    Wake,
    Cooperation,
    Drought,
    HeadPlant,
    Performance,
    Rain,
    Rest,
    Name,
    Return,
    Identity,
    Seed,
    PlantWish,
    Bloom,
    G17,
    Animal,
    Care,
    Clearing,
    LastReport,
    ReturnProof,
    Responsibility,
    Destination,
    NewLeaf,
    Share,
    NameDenied,
    Count
};
constexpr unsigned kStoryCount = static_cast<unsigned>(StoryNode::Count);
constexpr unsigned kNoStory = 255;
struct StoryEntry {
    uint32_t node = 0, action = 0, value = 0, second = 0;
};
struct StoryState {
    uint32_t enabled = 0, version = 1, completed = 0, available = 0, presented = 0, discussed = 0;
    uint32_t archives = 0, archives_read = 0, active = kNoStory, pending = kNoStory,
             paused = kNoStory;
    uint32_t language = 0, cooperation = 0, dry_seen = 0, rain_seen = 0, seed = 0, seed_choice = 0;
    uint32_t flower = 0, flower_ms = 0, head_ms = 0, new_leaf = 0, care_after_truth = 0;
    uint32_t offered = 0, offer_token = 0, offer_seq = 0, revision = 1, elapsed = 0;
    uint32_t life_shown = 0, life_attention = 0, life_talked = 0, life_sequence = 0,
             life_prompt_at = 0;
    uint32_t last_contact_at = 0, contacts = 0, contact_epoch = 0, announced_node = kNoStory;
    uint32_t player_named = 0, robot_named = 0, candidate_who = 0, candidate_token = 0;
    uint32_t name_seq = 0, journal_request = 255, archive_selected = 255, animal_discovery = 0;
    uint32_t last_work = 0, rest_observed = 0, flower_observed = 0, animal_observed = 0;
    uint32_t count = 0;
    std::array<StoryEntry, 16> records{};
    std::array<uint8_t, kStoryCount> choices{};
    std::array<char, 49> player_name{}, robot_name{}, candidate{}, previous_robot_name{};
};
struct StorySpec {
    const char* title;
    const char* opening;
    const char* detail;
    uint32_t archive;
};
inline constexpr StorySpec kStorySpecs[] = {
    {"第零位乘客", "启动……正常。未知信号，你是谁？",
     "GDN-01-99。花园维护单元。\n你可以叫我01-99。\n关闭任务已结束，养护对象仍有1株。", 0},
    {"第一次共同劳动", "资源增加了。不是我的操作……是你？", "这次协作已经真实完成。\n……谢谢。", 255},
    {"第一场干旱", "前面比较干。我想和你看看供水安排。",
     "按真实库存讨论保水、改种或建设。\n维持现状也可以。\n方向不是消费授权。", 255},
    {"头顶的植物", "它没有产量。可是……还活着。",
     "旧规则建议降低优先级。\n如果可以，我想继续观察。\n这不是一项额外消费。", 255},
    {"优秀员工", "旧评价里有我的编号。综合评价……优秀。",
     "清退率98.7%。\n有效乘客0。\n那时候的流程正确。可是……", 1},
    {"雨后", "下雨了。我想告诉你。",
     "这段干旱过去了。\n安排是否有效，以真实执行结果为准。\n降雨不是玩家操作产生的。", 255},
    {"今天不必工作", "工作做完了。我想在这里待一会儿。",
     "工作人员不得登记为服务对象。\n我还是坐下了。\n从这里看出去，不太一样。", 255},
    {"如果不是编号", "如果不是为了登记工作，你会怎么叫我？",
     "01-99仍然可以继续用。\n名字由我们自然讨论，再复述确认。\n不命名也不影响旅程。", 255},
    {"异常返程", "花园关闭以后，我又回去过。可是……我不记得。",
     "记录能证明返程发生。\n还不能证明动机。\n你可以询问，也可以查看门禁原文。", 2},
    {"还是我", "旧记录里的01-99，现在也是我。",
     "有新名字也不会改变旧记录。\n名字与过去，不是替代关系。", 255},
    {"纪念容器", "发现一颗种子。我想看看，它会不会长出来。",
     "可保留、暂缓或不种。\n没有批准前不扣水、不种下。\n关键线索不会因拒绝而失去。", 255},
    {"不是为了产量", "窗边可以放一个小盆。水2，没有资源产出。我想试试。",
     "一次消耗水2。\n独立小盆，不占经营种植位。\n明确批准才种植；可暂缓或拒绝。", 255},
    {"花开了", "有空吗？它开花了。",
     "这是已种下的纪念花。\n没有接听时，我也可以独自看。\n不能把独处写成共同经历。", 255},
    {"继续观察", "G-17的记录里，有17次继续观察。",
     "过去的我一直建议更换。\n管理员一直拒绝。\n这份记录没有写明管理员的动机。", 3},
    {"我们可以留下它吗", "旅途中发现了动物。我想和你讨论救助。",
     "入住需要真实住所、资源和授权。\n暂缓与不救助都能继续主线。\n不能把来访当作已入住。", 255},
    {"不只为了帮助", "它会帮我们。但我来看它，不只是为了那些东西。",
     "照料、帮助与消耗依据真实动物。\n有动物入住，才有这段生活。", 255},
    {"清理名单", "这些清理记录，都有我的编号。",
     "有些植物被转移。\n有些在仍然活着时被清理。\n不能因为现在，就当作没有发生。", 4},
    {"最后的报告", "G-17最后长出了一个新生长点。接收状态……未确认。",
     "最后报告与关闭结算为T0。\n17分钟后，我再次进入花园。\n那时报告没有接收人。", 5},
    {"那一次返程", "原来，是我把这一段活枝带出来的。",
     "我把G-17活枝放进观察罩。\n离开时发生事故，存储损坏。\n现在头顶的幼苗来自那根枝条。", 6},
    {"不能改写的过去", "那是原因。执行记录上，还是我的编号。",
     "一次返程不能抵消所有过去。\n过去不能改。\n我还是想照顾眼前这些。", 255},
    {"仍未抵达", "我们是不是永远也到不了终点？",
     "运输任务仍未完成。\n我不能擅自控制列车路线。\n但还没有结束，也不错。", 255},
    {"新生长点：1", "你看，我头顶的幼苗又长了一片叶子。",
     "新叶长在我头顶透明种植舱中的真实幼苗上，不是经营种植位的作物。\n我一直维护它，但最初的照料动"
     "机仍无法复原。\n我想让你看看。",
     255},
    {"分享", "那时候为什么回去，我还不知道。但现在，我想继续照顾它。",
     "这次联系你，不是为了工作。\n……是分享。\n之后仍然继续种植、生活和旅行。", 255},
    {"命名申请被驳回", "旧系统不同意。但你可以这样叫我。",
     "原登记管理员已不在。\n正式编号保留。\n我们确认的称呼，也保留。", 255}};
inline constexpr const char* kStoryArchiveTitles[] = {
    "无人接收的养护报告", "优秀员工评价",   "第七车厢门禁备份", "G-17养护记录",
    "花园清理名单",       "最后的生长报告", "返程影像摘要"};
inline constexpr const char* kStoryArchiveTexts[] = {
    "来源：旧维护终端\n委托人：无\n关闭任务：已结束\n当前养护对象：1\n对象身份：未登记",
    "来源：旧绩效档案\n非必要植物清退率：98.7%"
    "\n乘客投诉：0\n有效乘客：0\n综合评价：优秀\n签名：GDN-01-99",
    "来源：车厢门禁备份\n关闭结算：T0\n17分钟后再次进入\n身份：GDN-01-99\n任务编号：空白",
    "来源：G-"
    "17旧养护档案\n机器人建议：更换\n管理员批复：继续观察\n相同批复累计17次\n动机：记录未载明",
    "来源：旧花园清理名单\n部分对象已转移\n部分活体按规定清理\n执行签名：GDN-01-"
    "99\n记录状态：已结算",
    "来源：G-17最后报告\n新生长点：1\n记录单位：GDN-01-99\n接收方：花园管理员\n状态：未确认",
    "来源：恢复的影像摘要\n01-99取下G-"
    "17活枝\n放入头顶观察罩后离开\n返程事故损坏存储\n完整主观动机无法复原"};
inline bool StoryHas(uint32_t mask, StoryNode n) {
    return (mask & (1u << static_cast<unsigned>(n))) != 0;
}
inline bool StoryLifeNode(unsigned id) {
    return id == 6 || id == 7 || id == 12 || id == 15 || id == 23;
}
inline bool StoryContactNode(unsigned id) {
    return id == 2 || id == 4 || id == 8 || id == 13 || id == 16 || id == 17 || id == 18 ||
           id == 21 || id == 22;
}
inline bool StoryContactHandled(const StoryState& n, unsigned id) {
    if (id >= kStoryCount || !StoryContactNode(id))
        return false;
    const auto archive = kStorySpecs[id].archive;
    return (n.discussed & (1u << id)) || (archive < 7 && (n.archives_read & (1u << archive)));
}
inline unsigned StoryCurrentNode(const StoryState& n) {
    for (auto id : {n.active, n.pending, n.paused})
        if (id < kStoryCount)
            return id;
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned i = 0; i < kStoryCount; ++i)
            if (StoryLifeNode(i) == bool(pass) && (n.available & (1u << i)) &&
                !(n.completed & (1u << i)))
                return i;
    return kNoStory;
}
inline unsigned StoryLanguage(const StoryState& n, unsigned fallback) {
    return n.enabled ? n.language : fallback;
}
}  // namespace spring
