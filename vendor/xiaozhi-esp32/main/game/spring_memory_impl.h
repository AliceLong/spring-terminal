// Included inside Core. Copy-on-write keeps durable, monitor and MCP snapshots immutable.
MemoryData& WriteMemory() {
    if (s.memory.use_count() != 1)
        s.memory = std::make_shared<MemoryData>(*s.memory);
    return *s.memory;
}
uint32_t Remember(uint32_t category, MemoryAction action, MemoryActor actor, uint32_t kind = 0,
                  uint32_t item = 0, uint32_t value = 0, uint32_t result = 0, uint32_t chain = 0,
                  uint32_t cost = 0, uint32_t food = 0, uint32_t evidence = 0, uint32_t flags = 0) {
    if (!s.memory || s.memory->last_id >= UINT32_MAX - 1)
        return 0;
    auto& m = WriteMemory();
    if (m.count == m.records.size()) {
        std::move(m.records.begin() + 1, m.records.end(), m.records.begin());
        --m.count;
        ++m.dropped;
    }
    const auto id = ++m.last_id;
    m.records[m.count++] = {
        id,
        chain ? chain : id,
        static_cast<uint32_t>(s.game_ms / 1000),
        category,
        static_cast<uint32_t>(action),
        static_cast<uint32_t>(actor),
        kind,
        item,
        value,
        result,
        cost,
        food,
        evidence,
        flags | ((actor == MemoryActor::Player || actor == MemoryActor::VoiceRequest) ? 1u : 0u)};
    ++m.generation;
    ++s.revision;
    return id;
}
void PinMemory(uint32_t type, uint32_t event) {
    if (!s.memory || !event)
        return;
    for (unsigned i = 0; i < s.memory->fact_count; ++i)
        if (s.memory->facts[i].type == type)
            return;
    auto& m = WriteMemory();
    if (m.fact_count == m.facts.size()) {
        ++m.fact_overflow;
        ++m.generation;
        return;  // Never replace a permanent fact with an ordinary event.
    }
    const auto& e = m.records[m.count - 1];
    if (e.id != event)
        return;
    m.facts[m.fact_count++] = {type,
                               e.id,
                               e.chain,
                               e.second,
                               e.actor,
                               e.kind,
                               e.target,
                               e.value,
                               e.cost,
                               e.food_cost,
                               e.flags | (m.fresh_start ? 0u : 4u)};
}
void EnableMemory(uint32_t hi, uint32_t lo) {
    if (s.memory)
        return;
    s.memory = std::make_shared<MemoryData>();
    auto& m = *s.memory;
    m.world_hi = hi;
    m.world_lo = lo ? lo : 1;
    m.start_second = s.game_ms / 1000;
    m.fresh_start = !s.game_ms && !s.story_stage;
    auto id = Remember(5, MemoryAction::TrackingStarted, MemoryActor::System);
    PinMemory(1, id);
    // Retained old records are evidence, not reconstructed approvals or shared experiences.
    for (unsigned i = 0; i < s.event_count; ++i) {
        const auto r = s.events[i];
        Remember(5, MemoryAction::WorldEvent, MemoryActor::Unknown, static_cast<uint32_t>(r.type),
                 r.target, r.amount, 0, 0, 0, 0, r.id, 2);
        auto& imported = WriteMemory();
        imported.records[imported.count - 1].second = r.second;
        ++imported.imported;
    }
    for (unsigned i = 0; i < s.economy.decision_count; ++i) {
        const auto r = s.economy.decisions[i];
        bool duplicate = false;
        for (unsigned j = 0; j < s.event_count; ++j)
            duplicate |= s.events[j].id == r.id;
        if (duplicate)
            continue;
        Remember(5, MemoryAction::WorldEvent, MemoryActor::Unknown, static_cast<uint32_t>(r.type),
                 r.target, r.amount, 0, 0, 0, 0, r.id, 2);
        auto& imported = WriteMemory();
        imported.records[imported.count - 1].second = r.second;
        ++imported.imported;
    }
    for (unsigned i = 0; i < s.life.count; ++i) {
        const auto r = s.life.records[i];
        Remember(5, MemoryAction::Finished, MemoryActor::Robot, r.intent, r.target, r.plant,
                 r.result, 0, 0, 0, r.id, 2 | (r.reached ? 16u : 0u));
        auto& imported = WriteMemory();
        imported.records[imported.count - 1].second = r.second;
        ++imported.imported;
    }
    if (s.economy.proposal.kind) {
        const auto p = s.economy.proposal;
        const auto event = Remember(5, MemoryAction::Proposed, MemoryActor::Unknown, p.kind,
                                    p.target, p.value, 0, 0, 0, 0, 0, 8);
        WriteMemory().pending = {event, p.kind, p.target, p.value, 0, 0, 0, 0, p.token};
    }
    auto baseline_job = [&](unsigned slot, unsigned kind, unsigned target, unsigned value) {
        const auto event = Remember(5, MemoryAction::Started, MemoryActor::Unknown, kind, target,
                                    value, 0, 0, 0, 0, 0, 8);
        WriteMemory().jobs[slot] = {event, kind, target, value, 0, 0, 0, 0, 0};
    };
    if (s.economy.construction < 8)
        baseline_job(0, 1, s.economy.construction, 0);
    if (s.tree_work_ms)
        baseline_job(1, 7, 0, s.tree_stage);
    if (s.economy.recycle_ms)
        baseline_job(2, 6, 0, 0);
    if (s.economy.journey_work)
        baseline_job(3, 5, 0, s.economy.journey_task);
    for (unsigned i = 0; i < PlotCount(); ++i)
        if (PlotAt(i).plant != PlotAt(i).next)
            baseline_job(4 + i, 2, i, static_cast<unsigned>(PlotAt(i).next));
    // Baselines explicitly say 'already present when recording began', with unknown actor.
    if (s.tree_stage) {
        id = Remember(5, MemoryAction::TrackingStarted, MemoryActor::Unknown, 7, 0, s.tree_stage, 0,
                      0, 0, 0, 0, 8);
        PinMemory(40, id);
    }
    for (unsigned i = 0; i < 8; ++i)
        if (HasFacility(i)) {
            id = Remember(5, MemoryAction::TrackingStarted, MemoryActor::Unknown, 1, i, 0, 0, 0, 0,
                          0, 0, 8);
            PinMemory(60 + i, id);
        }
    for (unsigned i = 0; i < 2; ++i)
        if (s.economy.animals & (1u << i)) {
            id = Remember(5, MemoryAction::TrackingStarted, MemoryActor::Unknown, 5, i + 4, 0, 0, 0,
                          0, 0, 0, 8);
            PinMemory(70 + i, id);
        }
}
static uint32_t MemoryCost(uint32_t water, uint32_t energy, uint32_t fiber, uint32_t parts) {
    return water | (energy << 8) | (fiber << 16) | (parts << 24);
}
void MemoryProposed(const Proposal& p, MemoryActor actor) {
    if (!s.memory)
        return;
    const auto old = s.memory->pending;
    if (old.chain)
        Remember(2, MemoryAction::Superseded, actor, old.kind, old.target, old.value, 0, old.chain);
    const auto id = Remember(2, MemoryAction::Proposed, actor, p.kind, p.target, p.value);
    WriteMemory().pending = {id, p.kind, p.target, p.value, 0, 0, 0, 0, p.token};
}
void MemoryRejected(uint32_t token) {
    if (!s.memory || s.memory->pending.token != token)
        return;
    const auto p = s.memory->pending;
    Remember(2, MemoryAction::Rejected, MemoryActor::Player, p.kind, p.target, p.value, 0, p.chain);
    auto& m = WriteMemory();
    m.pending = {};
    ++m.totals[3];
}
MemoryJob MemoryApproved(const Proposal& p) {
    if (!s.memory)
        return {};
    auto chain = s.memory->pending.token == p.token ? s.memory->pending.chain : 0;
    uint32_t cost = 0, food = 0;
    if (p.kind == 1) {
        const auto& f = kFacilities[p.target];
        cost = MemoryCost(f.water, f.energy, f.fiber, f.parts);
    } else if (p.kind == 7)
        cost = MemoryCost(TreeWater(s.tree_stage), 0, TreeFiber(s.tree_stage), 0);
    else if (p.kind == 6)
        cost = MemoryCost(0, HasFacility(5) ? 2 : 0, 0, 0);
    else if (p.kind == 5 && p.value) {
        cost = MemoryCost(s.economy.journey >= 4 && s.economy.journey != 6 ? 2 : 0,
                          s.economy.journey == 1 ? 2 : 0, 0, 0);
        food = s.economy.journey == 4 ? 2 : s.economy.journey == 5 ? 4 : 0;
    }
    const auto id = Remember(2, MemoryAction::Approved, MemoryActor::Player, p.kind, p.target,
                             p.kind == 5 ? s.economy.journey : p.value, 0, chain, cost, food, 0, 1);
    if (!chain)
        chain = id;  // Direct approval / legacy pending proposal, no invented proposal event.
    MemoryJob job{chain, p.kind, p.target, p.kind == 5 ? s.economy.journey : p.value, 2, cost,
                  food,  0,      p.token};
    auto& m = WriteMemory();
    if (p.token && m.pending.token == p.token)
        m.pending = {};
    ++m.totals[2];
    unsigned slot = p.kind == 1   ? 0
                    : p.kind == 7 ? 1
                    : p.kind == 6 ? 2
                    : p.kind == 5 ? 3
                                  : 4 + p.target;
    if (p.kind == 1 || p.kind == 7 || p.kind == 6 || (p.kind == 5 && p.value) || p.kind == 2) {
        const auto previous = m.jobs[slot];
        if (previous.chain)
            Remember(2, MemoryAction::Superseded, MemoryActor::Player, previous.kind,
                     previous.target, previous.value, 0, previous.chain);
        WriteMemory().jobs[slot] = job;
    }
    return job;
}
uint32_t MemoryFinishJob(unsigned slot, uint32_t event = 0, uint32_t value = 0) {
    if (!s.memory || slot >= s.memory->jobs.size())
        return 0;
    const auto j = s.memory->jobs[slot];
    if (!j.chain)
        return 0;
    const auto id = Remember(2, MemoryAction::Completed, MemoryActor::Robot, j.kind, j.target,
                             value, 0, j.chain, j.cost, j.food_cost, event, j.actor == 2 ? 1 : 0);
    auto& m = WriteMemory();
    m.jobs[slot] = {};
    ++m.totals[6];
    return id;
}
void MemoryDecisionResult(const Proposal& p, const MemoryJob& job, bool immediate_plant) {
    if (!s.memory)
        return;
    auto action = p.kind == 2 ? MemoryAction::Scheduled : MemoryAction::Started;
    if (p.kind == 3 || p.kind == 4 || (p.kind == 5 && !p.value))
        action = MemoryAction::Completed;
    if (p.kind != 7)
        Remember(2, action, MemoryActor::Robot, p.kind, p.target, job.value,
                 p.kind == 5 && !p.value ? 1 : 0, job.chain, job.cost, job.food_cost, 0, 1);
    if (p.kind == 2 && immediate_plant)
        MemoryFinishJob(4 + p.target, 0, p.value);
}
void MemoryPlantApplied(unsigned slot) {
    if (!s.memory || slot >= 4)
        return;
    const auto job = s.memory->jobs[4 + slot];
    if (job.chain && static_cast<unsigned>(PlotAt(slot).plant) == job.value)
        MemoryFinishJob(4 + slot, 0, job.value);
}
void MemoryWorld(Event event, uint32_t item, uint32_t amount, uint32_t evidence,
                 MemoryActor actor) {
    if (!s.memory)
        return;
    if (actor == MemoryActor::Unknown &&
        (event == Event::Matured || event == Event::AnimalFed || event == Event::RegionChanged ||
         event == Event::EmergencyStarted || event == Event::EmergencyDone))
        actor = MemoryActor::System;
    Remember(3, MemoryAction::WorldEvent, actor, static_cast<unsigned>(event), item, amount, 0, 0,
             0, 0, evidence);
    ++WriteMemory().totals[7];
    if (event == Event::Harvest) {
        ++WriteMemory().totals[actor == MemoryActor::Player ? 4 : 5];
        if (actor == MemoryActor::Player && amount) {
            const auto id = Remember(4, MemoryAction::Completed, MemoryActor::Player, 0, item,
                                     amount, 0, 0, 0, 0, evidence, 1);
            PinMemory(2, id);
        }
    }
    if (event == Event::TreeStarted && s.memory->jobs[1].chain) {
        const auto j = s.memory->jobs[1];
        Remember(2, MemoryAction::Started, MemoryActor::Robot, 7, 0, s.tree_stage, 0, j.chain,
                 j.cost, j.food_cost, evidence, j.actor == 2 ? 1 : 0);
    }
    if (event == Event::FacilityBuilt) {
        auto id = MemoryFinishJob(0, evidence, item);
        if (!id)
            id = Remember(2, MemoryAction::Completed, MemoryActor::Unknown, 1, item, 0, 0, 0, 0, 0,
                          evidence);
        PinMemory(20 + item, id);
    }
    if (event == Event::TreeGrown) {
        auto id = MemoryFinishJob(1, evidence, amount);
        if (!id)
            id = Remember(2, MemoryAction::Completed, MemoryActor::Unknown, 7, 0, amount, 0, 0, 0,
                          0, evidence);
        PinMemory(2 + amount, id);
    }
    if (event == Event::Recycled)
        MemoryFinishJob(2, evidence, amount);
    if (event == Event::JourneyCompleted) {
        auto id = MemoryFinishJob(3, evidence, item);
        if (item == 4 || item == 5) {
            if (!id)
                id = Remember(2, MemoryAction::Completed, MemoryActor::Unknown, 5, item, item, 0, 0,
                              0, 0, evidence);
            PinMemory(item + 6, id);
        }
    }
}
void MemoryLife(MemoryAction action, uint32_t result = 0, MemoryActor actor = MemoryActor::Robot,
                ScheduleBlocker cause = ScheduleBlocker::None) {
    if (!s.memory || !s.life.intent)
        return;
    const auto l = s.life;
    uint32_t chain = s.memory->life_chain;
    if (action == MemoryAction::Requested || s.memory->live_life_sequence != l.sequence)
        chain = 0;
    const auto id = Remember(1, action, actor, l.intent, l.target,
                             l.plant | (l.reason << 8) | (unsigned(cause) << 16), result, chain, 0,
                             0, l.sequence, (l.reached ? 16u : 0u) | (l.under_tree ? 32u : 0u));
    auto& m = WriteMemory();
    m.life_chain = chain ? chain : id;
    m.live_life_sequence = l.sequence;
    if (action == MemoryAction::Finished) {
        ++m.totals[result == 0 ? 0 : 1];
        m.life_chain = m.live_life_sequence = 0;
    }
}
void MemoryTaskPauses() {
    if (!s.memory)
        return;
    for (unsigned i = 0; i < 4; ++i) {
        auto j = s.memory->jobs[i];
        if (!j.chain)
            continue;
        const unsigned pause = s.economy.emergency_work             ? 2
                               : !s.energy                          ? 1
                               : work == Work::Harvest && s.work_ms ? 3
                                                                    : 0;
        if (pause != j.pause) {
            Remember(2, pause ? MemoryAction::Paused : MemoryAction::Resumed, MemoryActor::System,
                     j.kind, j.target, j.value, pause, j.chain);
            WriteMemory().jobs[i].pause = pause;
        }
    }
}
bool MemoryLinksValid() const {
    if (!s.memory)
        return false;
    const auto& m = *s.memory;
    if (m.pending.chain &&
        (m.pending.token != s.economy.proposal.token || m.pending.kind != s.economy.proposal.kind ||
         m.pending.target != s.economy.proposal.target ||
         m.pending.value != s.economy.proposal.value))
        return false;
    if (m.jobs[0].chain && (m.jobs[0].kind != 1 || s.economy.construction != m.jobs[0].target))
        return false;
    if (m.jobs[1].chain && (m.jobs[1].kind != 7 || !s.tree_work_ms))
        return false;
    if (m.jobs[2].chain && (m.jobs[2].kind != 6 || !s.economy.recycle_ms))
        return false;
    if (m.jobs[3].chain && (m.jobs[3].kind != 5 || !s.economy.journey_work))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (m.jobs[4 + i].chain &&
            (i >= PlotCount() || m.jobs[4 + i].kind != 2 || m.jobs[4 + i].target != i ||
             m.jobs[4 + i].value != static_cast<unsigned>(PlotAt(i).next)))
            return false;
    return !m.life_chain || (s.life.intent && m.live_life_sequence == s.life.sequence);
}
std::unique_ptr<ArchiveSave> PackArchive() const {
    if (!s.memory)
        return nullptr;
    auto out = std::make_unique<ArchiveSave>();
    (*out)[0] = 0x4d454d35;
    (*out)[1] = 1;
    (*out)[2] = s.memory->generation;
    (*out)[3] = sizeof(MemoryData);
    const auto core = PackFull();
    std::copy(core.begin(), core.end(), out->begin() + 4);
    std::memcpy(out->data() + 388, s.memory.get(), sizeof(MemoryData));
    out->back() = FullChecksum(*out);
    return out;
}
bool RestoreArchive(const ArchiveSave& words) {
    if (words[0] != 0x4d454d35 || words[1] != 1 || words[3] != sizeof(MemoryData) ||
        words.back() != FullChecksum(words))
        return false;
    Core next;
    next.storage_ok = storage_ok;
    next.demo = demo;
    FullSave core{};
    std::copy_n(words.begin() + 4, core.size(), core.begin());
    if (!next.RestoreFull(core))
        return false;
    auto m = std::make_shared<MemoryData>();
    std::memcpy(m.get(), words.data() + 388, sizeof(MemoryData));
    if (m->version != 1 || m->generation != words[2] || !m->generation || !m->world_lo ||
        m->count > m->records.size() || m->fact_count > m->facts.size() || m->fresh_start > 1 ||
        m->start_second > next.s.game_ms / 1000)
        return false;
    uint32_t previous = 0;
    for (unsigned i = 0; i < m->count; ++i) {
        const auto& e = m->records[i];
        if (!e.id || e.id <= previous || e.id > m->last_id || !e.chain || e.chain > e.id ||
            e.second > next.s.game_ms / 1000 || e.category < 1 || e.category > 5 || e.action > 15 ||
            e.actor > 4 || e.flags > 63)
            return false;
        previous = e.id;
    }
    for (unsigned i = 0; i < m->fact_count; ++i) {
        const auto& f = m->facts[i];
        if (!f.type || !f.event || f.event > m->last_id || !f.chain || f.chain > f.event ||
            f.second > next.s.game_ms / 1000 || f.actor > 4 || f.flags > 63)
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (m->facts[j].type == f.type)
                return false;
    }
    auto valid_job = [&](const MemoryJob& j) {
        return !j.chain || (j.chain <= m->last_id && j.kind >= 1 && j.kind <= 7 && j.actor <= 4 &&
                            j.pause <= 3 && j.token <= next.s.economy.proposal_seq);
    };
    if (!valid_job(m->pending))
        return false;
    for (const auto& j : m->jobs)
        if (!valid_job(j))
            return false;
    next.s.memory = std::move(m);
    if (!next.MemoryLinksValid())
        return false;
    *this = std::move(next);
    return true;
}
CheckpointSave PackCheckpoint() const {
    CheckpointSave out{};
    out[0] = 0x43484b35;
    out[1] = 1;
    out[2] = s.memory ? s.memory->generation : 0;
    out[3] = s.memory ? s.memory->last_id : 0;
    out[4] = s.memory ? s.memory->world_hi : 0;
    out[5] = s.memory ? s.memory->world_lo : 0;
    const auto core = PackFull();
    std::copy(core.begin(), core.end(), out.begin() + 8);
    out.back() = FullChecksum(out);
    return out;
}
bool RestoreCheckpoint(const CheckpointSave& words) {
    if (!s.memory || words[0] != 0x43484b35 || words[1] != 1 || words[2] != s.memory->generation ||
        words[3] != s.memory->last_id || words[4] != s.memory->world_hi ||
        words[5] != s.memory->world_lo || words.back() != FullChecksum(words))
        return false;
    Core next;
    next.storage_ok = storage_ok;
    next.demo = demo;
    FullSave core{};
    std::copy_n(words.begin() + 8, core.size(), core.begin());
    if (!next.RestoreFull(core) || next.s.game_ms < s.game_ms ||
        (next.s.game_ms == s.game_ms && next.s.revision < s.revision))
        return false;
    next.s.memory = s.memory;
    if (!next.MemoryLinksValid())
        return false;
    *this = std::move(next);
    return true;
}
