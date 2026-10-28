// Included inside Core. Life uses real elapsed time, never offline/game-speed time.
bool LifeActive() const { return s.life.intent != 0; }
bool LifeWorkPending() const {
    return PlantWorkActive() || BusyEconomy() || s.tree_work_ms || work != Work::Rest || s.work_ms;
}
void FinishLife(LifeResult result, ScheduleBlocker cause = ScheduleBlocker::None) {
    auto& l = s.life;
    if (!l.intent)
        return;
    if (l.count == l.records.size()) {
        std::move(l.records.begin() + 1, l.records.end(), l.records.begin());
        --l.count;
    }
    l.records[l.count++] = {l.sequence,
                            l.intent,
                            l.target,
                            l.plant,
                            l.reason,
                            static_cast<uint32_t>(result),
                            static_cast<uint32_t>(s.game_ms / 1000),
                            l.under_tree,
                            l.reached};
    MemoryLife(MemoryAction::Finished, static_cast<uint32_t>(result), MemoryActor::Robot, cause);
    ChapterLifeFinished(result);
    s.schedule.last_cause = cause;
    s.schedule.last_sequence = l.sequence;
    s.schedule.last_result = unsigned(result);
    s.schedule.last_intent = l.intent;
    s.schedule.approved_token = s.schedule.approved_sequence = 0;
    s.schedule.pause = ScheduleBlocker::None;
    s.schedule.waiting_short = false;
    l.repeat_cooldown[l.intent - 1] = AttentionPacing(roadshow).repeat_ms;
    l.cooldown = AttentionPacing(roadshow).life_ms;
    l.intent = l.phase = l.reason = l.target = l.plant = l.under_tree = l.reached = 0;
    l.elapsed = l.travel_ms = l.pause_ms = 0;
    ++s.revision;
}
// Return control to existing automatic maintenance; never create or charge a job.
bool CanResumeWork() const {
    return storage_ok && s.story_stage && s.economy.enabled && !PlantWorkActive() &&
           (!s.story.enabled || s.story.active == kNoStory);
}
bool ResumeWork() {
    if (!CanResumeWork())
        return false;
    if (LifeActive())
        FinishLife(LifeResult::TargetChanged, ScheduleBlocker::PlayerActivity);
    s.life.cooldown = AttentionPacing(roadshow).life_ms;
    ++s.revision;
    return true;
}
bool CanInviteLife(unsigned intent, unsigned target = 0) const {
    if (!storage_ok || !s.story_stage || !s.economy.enabled || intent < 1 || intent > 3 ||
        (LifeWorkPending() && !ShortWorkFinishing()) ||
        (s.story.enabled && s.story.active != kNoStory &&
         !(intent == 3 && s.story.active == unsigned(StoryNode::Rest) && ChapterCanOfferSeat())) ||
        s.water < 6 || s.energy < 4 || s.life.sequence == UINT32_MAX ||
        (intent == 2 && target >= PlotCount() &&
         !(s.story.enabled &&
           ((target == 4 && s.story.flower) || (target == 5 && s.economy.animals) ||
            (target == 6 && ChapterPerformanceKnown())))))
        return false;
    if (LifeActive() && s.life.intent == intent && (intent != 2 || s.life.target == target))
        return false;  // Already doing this; do not duplicate the invitation.
    for (unsigned i = 0; i < PlotCount(); ++i)
        if (PlotAt(i).ready)
            return false;
    return true;
}
bool StartLifeRequest(unsigned intent, unsigned target, unsigned reason, MemoryActor actor) {
    if (!CanInviteLife(intent, target) ||
        (actor == MemoryActor::Robot && AgencyMode(s.story, intent, target) != 2))
        return false;
    // A requested destination supersedes the previous voluntary activity.
    if (LifeActive())
        FinishLife(LifeResult::TargetChanged, ScheduleBlocker::PlayerActivity);
    auto& l = s.life;
    l.intent = intent;
    l.reason = reason;
    l.phase = 1;
    l.target = intent == 2 ? target : 0;
    l.plant =
        intent == 2 && l.target < PlotCount() ? static_cast<unsigned>(PlotAt(l.target).plant) : 0;
    l.under_tree = intent == 3 ? (ChapterBench(s.chapters) ? 2 : s.tree_stage >= 1 ? 1 : 0) : 0;
    l.elapsed = l.travel_ms = l.pause_ms = l.reached = 0;
    s.schedule.waiting_short = ShortWorkFinishing();
    s.schedule.pause = ScheduleBlocker::None;
    ++l.sequence;
    MemoryLife(MemoryAction::Requested, 0, actor);
    ++s.revision;
    return true;
}
bool InviteLife(unsigned intent, unsigned target = 0,
                MemoryActor actor = MemoryActor::VoiceRequest) {
    if ((actor != MemoryActor::VoiceRequest && actor != MemoryActor::Player) ||
        !CanInviteLife(intent, target))
        return false;
    if (intent == 3 && ChapterCanOfferSeat() && !ChapterBench(s.chapters)) {
        s.chapters.flags |= BenchPermission;
        s.chapters.invitation_evidence = Remember(4, MemoryAction::Approved, actor, 93);
        PinMemory(93, s.chapters.invitation_evidence);
    }
    return StartLifeRequest(intent, target, static_cast<unsigned>(LifeReason::PlayerInvitation),
                            actor);
}
bool CanConsiderLocalMindLife(unsigned intent, unsigned target = 0,
                              unsigned cooldown_grace_ms = 0) const {
    if (LifeWorkPending() || LifeActive() || intent < 1 || intent > 3 ||
        s.life.cooldown > cooldown_grace_ms || !CanInviteLife(intent, target))
        return false;
    const auto& l = s.life;
    const bool resume =
        l.count && l.records[l.count - 1].intent == intent &&
        l.records[l.count - 1].target == (intent == 2 ? target : 0) &&
        l.records[l.count - 1].result == static_cast<unsigned>(LifeResult::WorkInterrupted);
    return !l.repeat_cooldown[intent - 1] || resume;
}
bool CanRequestLocalMindLife(unsigned intent, unsigned target = 0,
                             unsigned cooldown_grace_ms = 0) const {
    return AgencyMode(s.story, intent, target) == 2 &&
           CanConsiderLocalMindLife(intent, target, cooldown_grace_ms);
}
bool RequestLocalMindLife(unsigned intent, unsigned target = 0) {
    if (!CanRequestLocalMindLife(intent, target))
        return false;
    return StartLifeRequest(intent, target, intent, MemoryActor::Robot);
}
void StepLife(uint32_t real_ms, bool voice_busy, bool arrived, bool local_selection = true) {
    if (!storage_ok || !s.story_stage || !s.economy.enabled)
        return;
    auto& l = s.life;
    if (life_recovered) {
        life_recovered = false;
        FinishLife(LifeResult::Rebooted);
        return;
    }
    // A callback supplies at most one worker interval, not an offline catch-up.
    real_ms = std::min<uint32_t>(real_ms, 1000u);
    for (auto& v : l.repeat_cooldown)
        v -= std::min(v, real_ms);
    l.cooldown -= std::min(l.cooldown, real_ms);
    if (LifeActive()) {
        if (s.story.enabled && s.story.active != kNoStory &&
            !(l.intent == 3 && l.under_tree == 2 && s.story.active == unsigned(StoryNode::Rest))) {
            FinishLife(LifeResult::WorkInterrupted, ScheduleBlocker::Story);
            return;
        }
        if (LifeWorkPending()) {
            CoordinateLifeWork();
            if (!LifeActive())
                return;
            l.pause_ms = std::min<uint32_t>(60000u, l.pause_ms + real_ms);
            if (l.pause_ms >= 60000)
                FinishLife(LifeResult::Timeout, s.schedule.pause != ScheduleBlocker::None
                                                    ? s.schedule.pause
                                                    : BodyBlocker());
            return;
        }
        s.schedule.waiting_short = false;
        if (l.intent == 2 &&
            ((l.target < PlotCount() && static_cast<unsigned>(PlotAt(l.target).plant) != l.plant) ||
             (l.target >= PlotCount() &&
              !(s.story.enabled &&
                ((l.target == 4 && s.story.flower) || (l.target == 5 && s.economy.animals) ||
                 (l.target == 6 && ChapterPerformanceKnown())))))) {
            FinishLife(LifeResult::TargetChanged);
            return;
        }
        if (s.schedule.pause != ScheduleBlocker::None) {
            if (!ApprovedLife()) {
                FinishLife(LifeResult::TargetChanged, s.schedule.pause);
                return;
            }
            bool mature = false;
            for (unsigned i = 0; i < PlotCount(); ++i)
                mature |= PlotAt(i).ready;
            if (s.water < 6 || s.energy < 4 || mature) {
                l.pause_ms = std::min<uint32_t>(60000u, l.pause_ms + real_ms);
                if (l.pause_ms >= 60000)
                    FinishLife(LifeResult::Timeout, s.schedule.pause != ScheduleBlocker::None
                                                        ? s.schedule.pause
                                                        : BodyBlocker());
                return;
            }
            const auto cause = s.schedule.pause;
            s.schedule.pause = ScheduleBlocker::None;
            l.phase = 2;
            l.pause_ms = l.travel_ms = 0;
            MemoryLife(MemoryAction::Resumed, unsigned(cause));
            ++s.revision;
            return;  // Re-approach and confirm actual arrival, retain elapsed activity time.
        }
        // A player's explicit invitation is allowed to run while the call stays open.
        // Autonomous life still pauses for voice as before.
        if (voice_busy && l.reason != static_cast<unsigned>(LifeReason::PlayerInvitation)) {
            l.pause_ms = std::min<uint32_t>(60000u, l.pause_ms + real_ms);
            if (l.phase != 4) {
                l.phase = 4;
                MemoryLife(MemoryAction::Paused);
                ++s.revision;
            }
            if (l.pause_ms >= 60000)
                FinishLife(LifeResult::Timeout);
            return;
        }
        l.pause_ms = 0;
        if (l.phase == 1 || l.phase == 4) {
            const bool resumed = l.phase == 4;
            l.phase = 2;  // Resume by approaching again; never assume teleportation.
            MemoryLife(resumed ? MemoryAction::Resumed : MemoryAction::Traveling);
            ++s.revision;
        }
        if (l.phase == 2) {
            l.travel_ms += real_ms;
            if (arrived) {
                l.phase = 3;
                l.reached = 1;
                MemoryLife(MemoryAction::Arrived);
                ++s.revision;
            } else if (l.travel_ms >= 30000)
                FinishLife(LifeResult::Timeout);
            return;
        }
        if (l.phase == 3) {
            // The pose controller must still confirm the activity is actually presented.
            if (!arrived)
                return;
            l.elapsed += real_ms;
            if (l.elapsed >= 20000)
                FinishLife(LifeResult::Completed);
        }
        return;
    }
    if (TryChapterObservation(voice_busy))
        return;
    if (!local_selection || l.cooldown || voice_busy || LifeWorkPending() || s.water < 6 ||
        s.energy < 4)
        return;
    for (unsigned i = 0; i < PlotCount(); ++i)
        if (PlotAt(i).ready)
            return;  // Offer the mature crop cooperation window first.
    // Stable context-dependent variation, with repeat cooldowns; no fixed activity loop.
    unsigned choice = 0, best = 0;
    for (unsigned i = 1; i <= 3; ++i) {
        if (l.repeat_cooldown[i - 1] || AgencyMode(s.story, i, 0) != 2)
            continue;
        const unsigned score =
            ((s.game_ms / 1000 + l.sequence * 37 + i * 73) * (i * 17 + 11)) % 101 +
            (i == 3 && s.tree_stage ? 20 : 0);
        if (!choice || score > best) {
            choice = i;
            best = score;
        }
    }
    if (!choice || l.sequence == UINT32_MAX)
        return;
    l.intent = choice;
    l.phase = 1;
    l.reason = choice;
    l.target = choice == 2 ? (l.sequence + s.economy.region) % PlotCount() : 0;
    if (s.story.enabled && choice == 2 && AgencyLevel(s.story) >= 2) {
        if (s.story.flower && l.sequence % 3 == 0)
            l.target = 4;
        else if (s.economy.animals && l.sequence % 3 == 1)
            l.target = 5;
    }
    l.plant =
        choice == 2 && l.target < PlotCount() ? static_cast<unsigned>(PlotAt(l.target).plant) : 0;
    l.under_tree = choice == 3 ? (ChapterBench(s.chapters) ? 2 : s.tree_stage >= 1 ? 1 : 0) : 0;
    l.elapsed = l.travel_ms = l.pause_ms = l.reached = 0;
    ++l.sequence;
    MemoryLife(MemoryAction::Requested);
    ++s.revision;
}
