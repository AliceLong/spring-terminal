// Included inside spring::Core. Story effects commit through the same world worker.
void StoryRecord(unsigned node, unsigned action, unsigned value = 0) {
    auto& n = s.story;
    if (n.count == n.records.size()) {
        std::move(n.records.begin() + 1, n.records.end(), n.records.begin());
        --n.count;
    }
    n.records[n.count++] = {node, action, value, static_cast<uint32_t>(s.game_ms / 1000)};
    ++n.revision;
    ++s.revision;
}
void EnableStory() {
    s.story = {};
    s.story.enabled = 1;
    s.chapters = {};
    s.chapters.version = 1;
    StoryRecord(0, 0);
}
bool StoryDone(StoryNode id) const { return StoryHas(s.story.completed, id); }
void StoryOffer(StoryNode id) {
    auto& n = s.story;
    const auto i = static_cast<unsigned>(id), bit = 1u << i;
    if ((n.available | n.completed) & bit)
        return;
    n.available |= bit;
    const auto archive = kStorySpecs[i].archive;
    if (archive < 7)
        n.archives |= 1u << archive;
    StoryRecord(i, 1, archive);
}
void StoryWorldEvent(Event e, unsigned item, MemoryActor actor) {
    auto& n = s.story;
    if (!n.enabled)
        return;
    ChapterWorldEvent(e, item, actor);
    if (e == Event::Harvest && actor == MemoryActor::Player) {
        ++n.cooperation;
        StoryRecord(1, 2, item);
        if (s.chapters.version)
            ChapterFinishNode(StoryNode::Cooperation);
    }
    if (e == Event::RegionChanged) {
        if (item == 1)
            n.dry_seen = 1;
        if (item == 2 && n.dry_seen)
            n.rain_seen = 1;
    }
}
void TickStory(uint32_t real_ms, bool voice_busy = false) {
    auto& n = s.story;
    if (!n.enabled || !storage_ok || !s.story_stage)
        return;
    const uint32_t dt = std::min<uint32_t>(real_ms, 1000u);
    // Relative timers use unsigned subtraction, so uptime may wrap without freezing invitations.
    n.elapsed += dt;
    // Suppress duplicate legacy wake/maturity/tree narratives; retain real shortage/journey plans.
    s.notices &= ~7u;
    s.announced &= ~7u;
    if (s.economy.region == 1)
        n.dry_seen = 1;
    if (s.economy.region == 2 && n.dry_seen)
        n.rain_seen = 1;
    if (n.flower && n.flower < 4) {
        n.flower_ms = std::min<uint32_t>(360000u, n.flower_ms + dt);
        const unsigned phase = 1 + std::min<uint32_t>(3u, n.flower_ms / 120000u);
        if (phase != n.flower) {
            n.flower = phase;
            StoryRecord(12, 3, phase);
        }
    }
    if (!PlantWorkExecuting() && n.last_work == static_cast<unsigned>(Work::Water) &&
        work != Work::Water && StoryDone(StoryNode::Destination))
        n.care_after_truth = 1;
    n.last_work = static_cast<unsigned>(PlantWorkExecuting() ? Work::Rest : work);
    if (s.life.phase == 3 && s.life.reached) {
        if (s.life.intent == 3)
            n.rest_observed = 1;
        if (s.life.intent == 2 && s.life.target == 4)
            n.flower_observed = 1;
        if (s.life.intent == 2 && s.life.target == 5)
            n.animal_observed = 1;
        if (StoryDone(StoryNode::Destination) && s.life.intent == 2)
            n.care_after_truth = 1;
    }
    if (StoryDone(StoryNode::Destination) && n.care_after_truth && !n.new_leaf) {
        n.head_ms = std::min<uint32_t>(60000u, n.head_ms + dt);
        if (n.head_ms == 60000) {
            n.new_leaf = 1;
            StoryRecord(21, 3, 1);
        }
    }
    StoryOffer(StoryNode::Wake);
    if (n.cooperation)
        StoryOffer(StoryNode::Cooperation);
    if (ChapterHas(s.chapters, ProductionRecorded) && n.dry_seen)
        StoryOffer(StoryNode::Drought);
    if (ChapterHas(s.chapters, DroughtEnded) && ChapterHas(s.chapters, ProductionRecorded)) {
        StoryOffer(StoryNode::Performance);
    }
    if (ChapterCanOfferSeat())
        StoryOffer(StoryNode::Rest);
    if (ChapterHas(s.chapters, BenchRestCompleted))
        StoryOffer(StoryNode::Return);
    if (StoryDone(StoryNode::Cooperation) && n.dry_seen)
        StoryOffer(StoryNode::Drought);
    if (StoryDone(StoryNode::Drought) &&
        (!s.chapters.version || ChapterHas(s.chapters, BeyondWorkObserved) ||
         ChapterHas(s.chapters, LegacyChapterBaseline)))
        StoryOffer(StoryNode::HeadPlant);
    if (StoryDone(StoryNode::HeadPlant) &&
        (!s.chapters.version ||
         (ChapterHas(s.chapters, DroughtEnded) && ChapterHas(s.chapters, ProductionRecorded)) ||
         ChapterHas(s.chapters, LegacyChapterBaseline)))
        StoryOffer(StoryNode::Performance);
    if (StoryDone(StoryNode::Performance) && n.rain_seen) {
        StoryOffer(StoryNode::Rain);
        if (s.chapters.version && ChapterHas(s.chapters, DroughtEnded))
            ChapterFinishNode(StoryNode::Rain);
    }
    if (n.rest_observed && (!s.chapters.version || ChapterHas(s.chapters, BenchRestCompleted)))
        StoryOffer(StoryNode::Rest);
    if (StoryDone(StoryNode::Rest) && n.language >= 1)
        StoryOffer(StoryNode::Name);
    if (StoryDone(StoryNode::Rain) &&
        (!s.chapters.version || ChapterHas(s.chapters, BenchRestCompleted) ||
         ChapterHas(s.chapters, LegacyChapterBaseline)))
        StoryOffer(StoryNode::Return);
    if (StoryDone(StoryNode::Return))
        StoryOffer(StoryNode::Identity);
    if (StoryDone(StoryNode::Return)) {
        n.seed = std::max<uint32_t>(n.seed, 1u);
        StoryOffer(StoryNode::Seed);
    }
    if (n.seed_choice == 1 && !n.flower)
        StoryOffer(StoryNode::PlantWish);
    if (n.flower == 4)
        StoryOffer(StoryNode::Bloom);
    if (StoryDone(StoryNode::Seed))
        StoryOffer(StoryNode::G17);
    if (StoryDone(StoryNode::G17)) {
        // A guaranteed route-scan discovery is independent of the optional economy queue.
        // Receiving a signal is not rescue, ownership or resource approval.
        if (!n.animal_discovery)
            n.animal_discovery = 1;
        StoryOffer(StoryNode::Animal);
        if (n.animal_discovery == 1 && !s.economy.pending_journey && !s.economy.animals) {
            if (s.economy.journey != 4 && s.economy.journey != 5)
                s.economy.pending_journey = 4;
            n.animal_discovery = 2;  // Queue at most once, without replacing another discovery.
        }
    }
    if (s.economy.animals && n.animal_observed)
        StoryOffer(StoryNode::Care);
    if (StoryDone(StoryNode::Animal))
        StoryOffer(StoryNode::Clearing);
    if (StoryDone(StoryNode::Clearing))
        StoryOffer(StoryNode::LastReport);
    if (StoryDone(StoryNode::LastReport))
        StoryOffer(StoryNode::ReturnProof);
    if (StoryDone(StoryNode::ReturnProof))
        StoryOffer(StoryNode::Responsibility);
    if (StoryDone(StoryNode::Responsibility))
        StoryOffer(StoryNode::Destination);
    if (n.new_leaf)
        StoryOffer(StoryNode::NewLeaf);
    if (StoryDone(StoryNode::NewLeaf))
        StoryOffer(StoryNode::Share);
    if (n.robot_named)
        StoryOffer(StoryNode::NameDenied);
    n.pending = kNoStory;
    if (n.active < kStoryCount || voice_busy)
        return;
    // New invitations are spaced out. Explicitly requested summaries may continue sooner.
    if (n.last_contact_at && n.elapsed - n.last_contact_at < 120000u)
        return;
    if (work != Work::Rest || BusyEconomy() || s.tree_work_ms)
        return;
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned i = 0; i < kStoryCount; ++i) {
            const bool life = StoryLifeNode(i);
            if (life != bool(pass) || !(n.available & (1u << i)) || (n.completed & (1u << i)) ||
                StoryContactHandled(n, i))
                continue;
            if (i == 6 && s.chapters.version && ChapterBench(s.chapters) &&
                !ChapterHas(s.chapters, BenchRestCompleted))
                continue;  // The player already accepted; do not invite again during the action.
            const bool first_seat = i == 6 && ChapterCanOfferSeat() && !ChapterBench(s.chapters) &&
                                    !ChapterHas(s.chapters, BenchRestCompleted);
            if (life && !first_seat &&
                (n.elapsed - n.last_contact_at < 300000u ||
                 (n.elapsed - n.contact_epoch < 600000u && n.contacts >= 2) ||
                 (i == 6 && (!(n.life_shown & 1) || n.elapsed - n.life_prompt_at < 120000u))))
                continue;
            if (n.paused == i)
                continue;
            n.pending = i;
            return;
        }
}
bool AcceptStory(unsigned id) {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || id >= kStoryCount || !(n.available & (1u << id)) ||
        (n.completed & (1u << id)))
        return false;
    if (n.active < kStoryCount && n.active != id)
        return false;
    n.active = id;
    n.pending = kNoStory;
    n.paused = kNoStory;
    n.last_contact_at = n.elapsed;
    if (n.elapsed - n.contact_epoch >= 600000u) {
        n.contact_epoch = n.elapsed;
        n.contacts = 0;
    }
    if (id == 6 || id == 7 || id == 12 || id == 15 || id == 23)
        ++n.contacts;
    StoryRecord(id, 5);
    return true;
}
bool CompleteStory(unsigned id, unsigned choice = 0) {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || id >= kStoryCount ||
        (n.active != id && !(id == 11 && n.offered)) ||
        ((n.completed & (1u << id)) && !(id == 11 && n.offered)) || choice > 3)
        return false;
    if (id == unsigned(StoryNode::Rest) && s.chapters.version &&
        !ChapterHas(s.chapters, LegacyChapterBaseline) &&
        !ChapterHas(s.chapters, BenchRestCompleted) && !StoryDone(StoryNode::Rest))
        return false;  // A verbal acknowledgement is not sitting on the passenger seat.
    if (id == 10) {
        if (!choice)
            return false;
        n.seed_choice = choice;
    }
    if (id == 11) {
        if (!choice)
            return false;
        if (choice == 1)
            return false;
        n.offered = 0;
        n.seed_choice = choice;
    }
    n.choices[id] = choice;
    n.completed |= 1u << id;
    n.presented |= 1u << id;
    n.discussed |= 1u << id;
    if (n.active == id)
        n.active = kNoStory;
    if (id == 5)
        n.language = std::max<uint32_t>(1u, n.language);
    if (id == 6) {
        n.life_talked |= 1;
        n.life_attention |= 1;
    }
    if (id == 13)
        n.language = std::max<uint32_t>(2u, n.language);
    if (id == 19)
        n.language = 3;
    if (id == 7 && !n.robot_named)
        n.available &= ~(1u << 23);
    StoryRecord(id, 6, choice);
    TickStory(0);
    return true;
}
bool PauseStory(unsigned id) {
    auto& n = s.story;
    if (!storage_ok || n.active != id || id >= kStoryCount)
        return false;
    n.paused = id;
    n.active = kNoStory;
    n.pending = kNoStory;
    StoryRecord(id, 7);
    return true;
}
bool ReadStoryArchive(unsigned id) {
    auto& n = s.story;
    if (!storage_ok || id >= 7 || !(n.archives & (1u << id)))
        return false;
    n.archive_selected = id;
    n.journal_request = id;
    StoryRecord(id, 8);
    return true;
}
bool OfferStoryFlower() {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || !n.seed || n.flower || !StoryDone(StoryNode::Seed))
        return false;
    if (n.offered)
        return true;
    if (n.offer_seq == UINT32_MAX)
        return false;
    n.offered = 1;
    n.offer_token = ++n.offer_seq;
    n.seed_choice = 1;
    StoryOffer(StoryNode::PlantWish);
    StoryRecord(11, 9, n.offer_token);
    return true;
}
bool ApproveStoryFlower(unsigned token) {
    auto& n = s.story;
    if (!storage_ok || !n.offered || !token || token != n.offer_token || n.flower || s.water < 2)
        return false;
    s.water -= 2;
    n.flower = 1;
    n.flower_ms = 0;
    n.offered = 0;
    n.seed_choice = 1;
    n.completed |= 1u << 11;
    n.presented |= 1u << 11;
    n.discussed |= 1u << 11;
    if (n.active == 11)
        n.active = kNoStory;
    StoryRecord(11, 10, 2);
    TickStory(0);
    return true;
}
static bool ValidStoryName(const char* text) {
    if (!text)
        return false;
    size_t length = std::strlen(text);
    if (!length || length > 48)
        return false;
    for (size_t i = 0; i < length;) {
        unsigned char c = text[i];
        if (c < 32 || c == 127)
            return false;
        size_t count =
            c < 128 ? 1
                    : (c >= 0xc2 && c <= 0xdf
                           ? 2
                           : (c >= 0xe0 && c <= 0xef ? 3 : (c >= 0xf0 && c <= 0xf4 ? 4 : 0)));
        if (!count || i + count > length)
            return false;
        for (size_t j = 1; j < count; ++j)
            if ((static_cast<unsigned char>(text[i + j]) & 0xc0) != 0x80)
                return false;
        if (count == 3 && ((c == 0xe0 && static_cast<unsigned char>(text[i + 1]) < 0xa0) ||
                           (c == 0xed && static_cast<unsigned char>(text[i + 1]) >= 0xa0)))
            return false;
        if (count == 4 && ((c == 0xf0 && static_cast<unsigned char>(text[i + 1]) < 0x90) ||
                           (c == 0xf4 && static_cast<unsigned char>(text[i + 1]) >= 0x90)))
            return false;
        i += count;
    }
    return true;
}
bool ProposeStoryName(unsigned who, const char* name) {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || who < 1 || who > 2 || !ValidStoryName(name) ||
        n.name_seq == UINT32_MAX)
        return false;
    if (n.candidate_who == who && std::strcmp(n.candidate.data(), name) == 0)
        return true;
    n.candidate = {};
    std::memcpy(n.candidate.data(), name, std::strlen(name));
    n.candidate_who = who;
    n.candidate_token = ++n.name_seq;
    StoryRecord(7, 11, who);
    return true;
}
bool ConfirmStoryName(unsigned token) {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || !n.candidate_who || token != n.candidate_token ||
        !ValidStoryName(n.candidate.data()))
        return false;
    if (n.candidate_who == 1) {
        n.player_name = n.candidate;
        n.player_named = 1;
    } else {
        n.previous_robot_name = n.robot_name;
        n.robot_name = n.candidate;
        n.robot_named = 1;
    }
    const auto who = n.candidate_who;
    n.candidate_who = 0;
    n.candidate = {};
    StoryRecord(7, 12, who);
    TickStory(0);
    return true;
}
bool NoticeStoryLife() {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || s.life.phase != 3 || !s.life.reached)
        return false;
    n.life_attention |= s.life.intent == 3   ? 1u
                        : s.life.target == 4 ? 2u
                        : s.life.target == 5 ? 4u
                                             : 8u;
    if (s.life.intent == 3 && !StoryDone(StoryNode::Rest) &&
        (!s.chapters.version || ChapterCanOfferSeat())) {
        StoryOffer(StoryNode::Rest);
        return AcceptStory(6);
    }
    StoryRecord(6, 13, s.life.intent);
    return true;
}

bool StoryPresented(unsigned id) {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || id >= kStoryCount || !(n.available & (1u << id)))
        return false;
    if (!(n.presented & (1u << id))) {
        n.presented |= 1u << id;
        StoryRecord(id, 15);
    }
    return true;
}
bool EndStoryCommunication(unsigned id) {
    auto& n = s.story;
    if (!storage_ok || !n.enabled || id >= kStoryCount || n.active != id)
        return false;
    // A delivered conversation consumes its invitation, not a narrative choice,
    // construction approval, physical action or the whole chapter.
    if (n.presented & (1u << id))
        n.discussed |= 1u << id;
    n.active = n.pending = kNoStory;
    n.last_contact_at = n.elapsed;
    // Reuse existing record codes so old save readers remain compatible.
    StoryRecord(id, (n.presented & (1u << id)) ? 15 : 7,
                (n.presented & (1u << id)) ? 1 : 0);
    return true;
}
bool StoryArchiveSeen(unsigned id) {
    if (!storage_ok || id >= 7 || !(s.story.archives & (1u << id)))
        return false;
    if (!(s.story.archives_read & (1u << id))) {
        s.story.archives_read |= 1u << id;
        StoryRecord(id, 14);
        if (id == 1 && s.chapters.version)
            ChapterFinishNode(StoryNode::Performance);
    }
    return true;
}
bool ShowStoryLife(unsigned sequence) {
    if (!storage_ok || s.life.sequence != sequence || s.life.phase != 3 || !s.life.reached)
        return false;
    auto& n = s.story;
    const unsigned bit = s.life.intent == 3   ? 1
                         : s.life.target == 4 ? 2
                         : s.life.target == 5 ? 4
                                              : 8;
    if (!(n.life_shown & bit)) {
        n.life_shown |= bit;
        n.life_prompt_at = n.elapsed;
        StoryRecord(6, 4, bit);
    }
    return true;
}
static uint32_t StoryChecksum(const StorySave& save) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&save);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(StorySave, checksum); ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}
std::unique_ptr<StorySave> PackStory() const {
    if (!s.story.enabled || !s.memory)
        return nullptr;
    auto out = std::make_unique<StorySave>();
    out->core = PackFull();
    out->story = s.story;
    out->memory = *s.memory;
    out->checksum = StoryChecksum(*out);
    return out;
}
bool RestoreStory(const StorySave& saved) {
    if (saved.magic != 0x53543636 || saved.version != 1 || saved.checksum != StoryChecksum(saved))
        return false;
    const auto& n = saved.story;
    constexpr uint32_t mask = (1u << kStoryCount) - 1;
    if (n.enabled != 1 || n.version != 1 || n.completed & ~mask || n.available & ~mask ||
        n.presented & ~mask || n.discussed & ~mask || n.completed & ~n.available ||
        n.language > 3 || n.archives > 127 || n.archives_read & ~n.archives || n.seed > 1 ||
        n.seed_choice > 3 || n.flower > 4 || n.flower_ms > 360000 || n.head_ms > 60000 ||
        n.new_leaf > 1 || n.offered > 1 || n.count > 16 || n.candidate_who > 2 ||
        n.player_named > 1 || n.robot_named > 1 || n.animal_discovery > 2)
        return false;
    for (auto id : {n.active, n.pending, n.paused})
        if (id != kNoStory &&
            (id >= kStoryCount || !(n.available & (1u << id)) || (n.completed & (1u << id))))
            return false;
    for (const auto* name : {&n.player_name, &n.robot_name, &n.candidate, &n.previous_robot_name})
        if (!std::memchr(name->data(), 0, name->size()))
            return false;
    if ((n.player_named && !ValidStoryName(n.player_name.data())) ||
        (n.robot_named && !ValidStoryName(n.robot_name.data())) ||
        (n.candidate_who && !ValidStoryName(n.candidate.data())))
        return false;
    auto archive = std::make_unique<ArchiveSave>();
    (*archive)[0] = 0x4d454d35;
    (*archive)[1] = 1;
    (*archive)[2] = saved.memory.generation;
    (*archive)[3] = sizeof(MemoryData);
    std::copy(saved.core.begin(), saved.core.end(), archive->begin() + 4);
    std::memcpy(archive->data() + 388, &saved.memory, sizeof(MemoryData));
    archive->back() = FullChecksum(*archive);
    Core next;
    next.storage_ok = storage_ok;
    next.demo = demo;
    if (!next.RestoreArchive(*archive))
        return false;
    next.s.story = n;
    if (!saved.core[326])
        next.InitChaptersFromLegacy();
    for (auto evidence : {next.s.chapters.drought_chain, next.s.chapters.observation_evidence,
                          next.s.chapters.bench_evidence, next.s.chapters.invitation_evidence})
        if (evidence > saved.memory.last_id)
            return false;
    if (next.s.life.under_tree == 2 && !ChapterBench(next.s.chapters))
        return false;
    if (next.s.life.intent == 2 && next.s.life.target == 6 && !next.ChapterPerformanceKnown())
        return false;
    if ((n.flower && n.seed_choice != 1) ||
        (next.s.life.intent == 2 && next.s.life.target == 4 && !n.flower) ||
        (next.s.life.intent == 2 && next.s.life.target == 5 && !next.s.economy.animals))
        return false;
    for (auto choice : n.choices)
        if (choice > 3)
            return false;
    for (unsigned i = 0; i < n.count; ++i)
        if (n.records[i].node >= kStoryCount || n.records[i].action > 15 ||
            n.records[i].second > next.s.game_ms / 1000)
            return false;
    *this = std::move(next);
    return true;
}

static uint32_t StoryCheckpointChecksum(const StoryCheckpoint& save) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&save);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(StoryCheckpoint, checksum); ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}
std::unique_ptr<StoryCheckpoint> PackStoryCheckpoint() const {
    if (!s.memory || !s.story.enabled)
        return nullptr;
    auto out = std::make_unique<StoryCheckpoint>();
    out->generation = s.memory->generation;
    out->last_id = s.memory->last_id;
    out->world_hi = s.memory->world_hi;
    out->world_lo = s.memory->world_lo;
    out->core = PackFull();
    out->story = s.story;
    out->checksum = StoryCheckpointChecksum(*out);
    return out;
}
bool RestoreStoryCheckpoint(const StoryCheckpoint& save) {
    if (!s.memory || save.magic != 0x53434b36 || save.version != 1 ||
        save.checksum != StoryCheckpointChecksum(save) || save.generation != s.memory->generation ||
        save.last_id != s.memory->last_id || save.world_hi != s.memory->world_hi ||
        save.world_lo != s.memory->world_lo)
        return false;
    auto archive = PackStory();
    if (!archive)
        return false;
    archive->core = save.core;
    archive->story = save.story;
    archive->checksum = StoryChecksum(*archive);
    Core next;
    next.storage_ok = storage_ok;
    next.demo = demo;
    if (!next.RestoreStory(*archive) || next.s.game_ms < s.game_ms)
        return false;
    *this = std::move(next);
    return true;
}
