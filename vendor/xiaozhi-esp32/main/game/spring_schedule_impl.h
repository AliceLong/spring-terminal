// Included inside Core. Sources and permissions are runtime facts, not model claims.
bool ApprovedLife() const {
    return LifeActive() && s.schedule.approved_token &&
           s.schedule.approved_sequence == s.life.sequence && AgencyLevel(s.story) == 1;
}
bool PlayerLifeReserved() const {
    return LifeActive() && s.life.reason == unsigned(LifeReason::PlayerInvitation) &&
           !ApprovedLife();
}
bool ShortWorkFinishing() const {
    return !BusyEconomy() && !s.tree_work_ms && !PlantWorkActive() && s.work_ms &&
           (work == Work::Water || work == Work::Harvest);
}
ScheduleBlocker BodyBlocker() const {
    if (s.economy.emergency_work)
        return ScheduleBlocker::Emergency;
    if (s.story.enabled && s.story.active != kNoStory &&
        !(s.life.intent == 3 && s.life.under_tree == 2 &&
          s.story.active == unsigned(StoryNode::Rest)))
        return ScheduleBlocker::Story;
    if (s.economy.construction != 255)
        return ScheduleBlocker::Construction;
    if (s.economy.journey_work)
        return ScheduleBlocker::Exploration;
    if (s.economy.recycle_ms)
        return ScheduleBlocker::Recycling;
    if (s.tree_work_ms)
        return ScheduleBlocker::TreeWork;
    if (PlantWorkActive())
        return ScheduleBlocker::PlayerTask;
    if (work == Work::Water && s.work_ms)
        return ScheduleBlocker::WaterFinishing;
    if (work == Work::Harvest && s.work_ms)
        return ScheduleBlocker::HarvestFinishing;
    return ScheduleBlocker::None;
}
void MarkApprovedLife(unsigned token) {
    s.schedule.approved_token = token;
    s.schedule.approved_sequence = s.life.sequence;
}
void PauseApprovedLife(ScheduleBlocker cause) {
    if (!ApprovedLife())
        return;
    if (s.schedule.pause == ScheduleBlocker::None) {
        s.schedule.pause = cause;
        s.life.phase = 4;  // Existing save-compatible pause, annotated by transient cause.
        s.life.pause_ms = 0;
        MemoryLife(MemoryAction::Paused, unsigned(cause));
        ++s.revision;
    }
}
// Called both at a command safe point and by the online life tick.
void CoordinateLifeWork() {
    if (!LifeActive() || !LifeWorkPending())
        return;
    if (s.schedule.waiting_short && s.life.phase == 1 && ShortWorkFinishing())
        return;
    const auto cause = BodyBlocker();
    if (ApprovedLife() &&
        (cause == ScheduleBlocker::PlayerTask || cause == ScheduleBlocker::HarvestFinishing ||
         cause == ScheduleBlocker::WaterFinishing)) {
        PauseApprovedLife(cause);
        return;
    }
    FinishLife(LifeResult::WorkInterrupted, cause);
}
bool CancelGardenerActivity(uint32_t plant_id, uint32_t life_sequence) {
    if (!storage_ok || bool(plant_id) == bool(life_sequence))
        return false;
    if (plant_id && PlantWorkActive() && plant_id == s.plant_action.id) {
        FinishPlantWork(PlantActionStatus::Cancelled, PlantActionReason::Interrupted);
        return true;
    }
    if (life_sequence && LifeActive() && life_sequence == s.life.sequence) {
        FinishLife(LifeResult::TargetChanged, ScheduleBlocker::PlayerActivity);
        return true;
    }
    return false;
}
