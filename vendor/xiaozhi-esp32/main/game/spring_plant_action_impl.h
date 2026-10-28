// Included inside Core. No network, UI, extra plant payment or growth bonus.
bool PlantWorkActive() const { return PlantActionActive(s.plant_action); }
bool PlantWorkExecuting() const {
    return s.plant_action.status == PlantActionStatus::Traveling ||
           s.plant_action.status == PlantActionStatus::Running;
}
PlantActionReason CheckPlantWork(unsigned kind, unsigned slot, uint32_t cycle) const {
    if (!storage_ok)
        return PlantActionReason::Storage;
    if (!s.story_stage || !s.economy.enabled || (s.story.enabled && s.story.active != kNoStory))
        return PlantActionReason::Unavailable;
    if (kind < 1 || kind > 2 || slot >= PlotCount())
        return PlantActionReason::InvalidTarget;
    const auto& p = PlotAt(slot);
    if (p.cycle != cycle)
        return PlantActionReason::StaleCycle;
    if (PlayerLifeReserved())
        return PlantActionReason::PlayerActivity;
    if (BusyEconomy() || s.tree_work_ms)
        return PlantActionReason::Busy;
    if (kind == 2)
        return p.ready ? PlantActionReason::None : PlantActionReason::NotMature;
    if (p.ready)
        return PlantActionReason::AlreadyMature;
    if (!(s.economy.paid_mask & (1u << slot)))
        return PlantActionReason::UnpaidCycle;
    return s.energy ? PlantActionReason::None : PlantActionReason::NoEnergy;
}
void PlantWorkMemory(MemoryAction stage, PlantActionReason reason = PlantActionReason::None) {
    auto& a = s.plant_action;
    const auto record = Remember(
        4, stage, stage == MemoryAction::Requested ? MemoryActor::VoiceRequest : MemoryActor::Robot,
        79 + a.kind, a.slot, a.cycle, static_cast<unsigned>(reason), a.chain);
    if (!a.chain)
        a.chain = record;
}
void FinishPlantWork(PlantActionStatus status, PlantActionReason reason) {
    auto& a = s.plant_action;
    const bool owned_body = PlantWorkExecuting();
    a.status = status;
    a.reason = reason;
    if (status == PlantActionStatus::Completed)
        ChapterPlantCompleted();
    if (status == PlantActionStatus::Completed && a.kind == 1 && s.story.enabled &&
        StoryDone(StoryNode::Destination))
        s.story.care_after_truth = 1;
    PlantWorkMemory(
        status == PlantActionStatus::Completed ? MemoryAction::Completed : MemoryAction::Finished,
        reason);
    if (owned_body) {
        work = Work::Rest;
        s.work_ms = 0;
    }
    ++s.revision;
}
bool RequestPlantWork(uint32_t id, unsigned kind, unsigned slot, uint32_t cycle) {
    if (!id || PlantWorkActive() || CheckPlantWork(kind, slot, cycle) != PlantActionReason::None)
        return false;
    if (ApprovedLife())
        PauseApprovedLife(ScheduleBlocker::PlayerTask);
    else if (LifeActive()) {
        FinishLife(LifeResult::WorkInterrupted, ScheduleBlocker::PlayerTask);
    }
    s.life.cooldown = 120000;
    s.plant_action = {id, kind, slot, cycle, PlantActionStatus::Queued};
    s.plant_action.wait = work == Work::Water && s.work_ms     ? ScheduleBlocker::WaterFinishing
                          : work == Work::Harvest && s.work_ms ? ScheduleBlocker::HarvestFinishing
                                                               : ScheduleBlocker::None;
    PlantWorkMemory(MemoryAction::Requested);
    ++s.revision;
    return true;
}
// Physical progress uses online REAL time and observed body pose, never offline simulation.
void StepPlantWork(uint32_t real_ms, bool arrived) {
    if (!PlantWorkActive())
        return;
    auto& a = s.plant_action;
    const auto reason = CheckPlantWork(a.kind, a.slot, a.cycle);
    if (reason != PlantActionReason::None) {
        FinishPlantWork(PlantActionStatus::Failed, reason);
        return;
    }
    a.elapsed_ms += std::min<uint32_t>(real_ms, 1000u);
    if (a.elapsed_ms >= 60000) {
        FinishPlantWork(PlantActionStatus::Failed, PlantActionReason::Timeout);
        return;
    }
    if (a.status == PlantActionStatus::Queued) {
        a.wait = work == Work::Water && s.work_ms     ? ScheduleBlocker::WaterFinishing
                 : work == Work::Harvest && s.work_ms ? ScheduleBlocker::HarvestFinishing
                                                      : ScheduleBlocker::None;
        if (s.work_ms || work != Work::Rest)
            return;  // Existing short tool animation finishes first.
        a.wait = ScheduleBlocker::None;
        a.status = PlantActionStatus::Traveling;
        work = a.kind == 2 ? Work::Harvest : Work::Water;
        target = a.slot;
        PlantWorkMemory(MemoryAction::Traveling);
        return;
    }
    if (s.energy)
        s.economy.round_worked = true;
    if (!arrived)
        return;
    if (a.status == PlantActionStatus::Traveling) {
        a.status = PlantActionStatus::Running;
        a.arrived = true;
        PlantWorkMemory(MemoryAction::Arrived);
        return;
    }
    a.work_ms += std::min<uint32_t>(real_ms, 1000u);
    if (a.work_ms < (a.kind == 2 ? 2000u : 5000u))
        return;
    if (a.kind == 2) {
        const auto& p = PlotAt(a.slot);
        const auto pool = [&]() {
            return p.plant == Plant::Light  ? s.energy
                   : p.plant == Plant::Moss ? s.water
                   : p.plant == Plant::Vine ? s.fiber
                                            : s.economy.food;
        };
        const auto before = pool();
        // Sole reward transaction, shared with automatic / physical harvesting.
        // It checks readiness again and consumes this exact crop cycle only once.
        const auto plant = p.plant;
        if (!EconomyHarvest(a.slot, false, MemoryActor::VoiceRequest)) {
            FinishPlantWork(PlantActionStatus::Failed, PlantActionReason::Interrupted);
            return;
        }
        const auto after = plant == Plant::Light  ? s.energy
                           : plant == Plant::Moss ? s.water
                           : plant == Plant::Vine ? s.fiber
                                                  : s.economy.food;
        a.amount = after - before;
    }
    FinishPlantWork(PlantActionStatus::Completed, PlantActionReason::None);
}
// Retained lifecycle records survive reboot; incomplete actions become interrupted,
// never resumed with a second payment/reward or marked completed by elapsed wall time.
void RecoverPlantWork() {
    if (!s.memory)
        return;
    // At most one accepted body task can be unfinished. Scan newest requests first;
    // do not append while traversing a full ring (append shifts all records).
    for (unsigned i = s.memory->count; i > 0; --i) {
        const auto e = s.memory->records[i - 1];
        if (e.category != 4 || (e.kind != 80 && e.kind != 81) ||
            e.action != unsigned(MemoryAction::Requested))
            continue;
        bool terminal = false;
        for (unsigned j = i; j < s.memory->count; ++j) {
            const auto& n = s.memory->records[j];
            if (n.chain == e.chain && (n.action == unsigned(MemoryAction::Completed) ||
                                       n.action == unsigned(MemoryAction::Finished)))
                terminal = true;
        }
        if (!terminal)
            Remember(4, MemoryAction::Finished, MemoryActor::System, e.kind, e.target, e.value,
                     unsigned(PlantActionReason::Restarted), e.chain);
        return;
    }
}
