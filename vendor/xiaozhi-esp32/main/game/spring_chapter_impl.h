// Included inside Core. Observation never spends resources or synthesizes outcomes.
void ChapterFinishNode(StoryNode id, uint32_t evidence = 0) {
    const auto node = unsigned(id), bit = 1u << node;
    auto& n = s.story;
    if (n.completed & bit)
        return;
    n.available |= bit;
    n.completed |= bit;
    if (n.active == node)
        n.active = kNoStory;
    if (n.pending == node)
        n.pending = kNoStory;
    if (n.paused == node)
        n.paused = kNoStory;
    StoryRecord(node, 3, evidence);  // Physical/world/read result, never an invented dialogue.
}
bool ChapterPerformanceKnown() const {
    return (s.story.archives_read & 2u) || StoryDone(StoryNode::Performance);
}
void InitChaptersFromLegacy() {
    if (!s.story.enabled || s.chapters.version)
        return;
    auto& c = s.chapters;
    c.version = 1;
    // Preserve completed old chapters without replaying or inventing a drought summary.
    if (s.story.rain_seen || StoryDone(StoryNode::Performance) ||
        (s.story.available & (1u << unsigned(StoryNode::Rest))))
        c.flags |= LegacyChapterBaseline;
    if (StoryDone(StoryNode::Performance))
        c.flags |= BeyondWorkObserved;
    // Old ground rest is not proof that the passenger seat was used.
}
uint32_t ChapterRecord(unsigned kind, unsigned target = 0, uint32_t value = 0,
                       MemoryActor actor = MemoryActor::System, uint32_t evidence = 0) {
    const auto id =
        Remember(4, MemoryAction::Completed, actor, kind, target, value, 0, 0, 0, 0, evidence);
    if (kind >= 90 && kind <= 93)
        PinMemory(kind, id);
    return id;
}
void ChapterWorldEvent(Event e, unsigned item, MemoryActor actor) {
    auto& c = s.chapters;
    if (!s.story.enabled || c.version != 1)
        return;
    if (e == Event::Harvest)
        c.flags |= ProductionRecorded;
    if (e == Event::RegionChanged && item == 1 && !ChapterHas(c, DroughtStarted) &&
        !ChapterHas(c, LegacyChapterBaseline)) {
        c.flags |= DroughtStarted;
        c.start_second = s.game_ms / 1000;
        c.start_water = s.water;
        c.start_energy = s.energy;
        c.drought_chain = Remember(4, MemoryAction::Started, MemoryActor::System, 90);
    }
    if (e == Event::RegionChanged && item != 1 && ChapterHas(c, DroughtStarted) &&
        !ChapterHas(c, DroughtEnded)) {
        c.flags |= DroughtEnded;
        c.end_second = s.game_ms / 1000;
        c.end_water = s.water;
        c.end_energy = s.energy;
        const auto id = Remember(4, MemoryAction::Completed, MemoryActor::System, 90, 0,
                                 c.end_water, 0, c.drought_chain);
        PinMemory(90, id);
        ChapterFinishNode(StoryNode::Drought, id);
    }
    if (e == Event::Harvest && ChapterHas(c, DroughtStarted) && !ChapterHas(c, DroughtEnded)) {
        if (actor == MemoryActor::Player)
            ++c.manual_harvests;
        else if (actor == MemoryActor::Robot)
            ++c.automatic_harvests;
    }
}
void ChapterPlantCompleted() {
    auto& c = s.chapters;
    if (c.version != 1 || !ChapterHas(c, DroughtStarted) || ChapterHas(c, DroughtEnded))
        return;
    if (s.plant_action.kind == 1)
        ++c.requested_cares;
    else
        ++c.requested_harvests;
    c.last_requested_slot = s.plant_action.slot;
}
void ChapterLifeFinished(LifeResult result) {
    auto& c = s.chapters;
    const auto& l = s.life;
    if (c.version != 1 || result != LifeResult::Completed || !l.reached)
        return;
    if (l.intent == 2 && l.target == 6 && l.sequence == c.observation_sequence &&
        !ChapterHas(c, BeyondWorkObserved)) {
        c.flags |= BeyondWorkObserved;
        c.observation_evidence = ChapterRecord(91, 6, l.sequence);
        ChapterFinishNode(StoryNode::HeadPlant, c.observation_evidence);
    }
    if (l.intent == 3 && l.under_tree == 2 && ChapterBench(c) &&
        !ChapterHas(c, BenchRestCompleted)) {
        c.flags |= BenchRestCompleted;
        c.bench_sequence = l.sequence;
        c.bench_evidence =
            ChapterRecord(92, 0, l.sequence, MemoryActor::Robot, c.invitation_evidence);
        s.story.rest_observed = 1;
        s.story.language = std::max<uint32_t>(1, s.story.language);
        // A physical result, not an invented player dialogue/choice.
        s.story.available |= 1u << unsigned(StoryNode::Rest);
        s.story.completed |= 1u << unsigned(StoryNode::Rest);
        if (s.story.active == unsigned(StoryNode::Rest))
            s.story.active = kNoStory;
        if (s.story.pending == unsigned(StoryNode::Rest))
            s.story.pending = kNoStory;
        if (s.story.paused == unsigned(StoryNode::Rest))
            s.story.paused = kNoStory;
        StoryRecord(unsigned(StoryNode::Rest), 3, l.sequence);
    }
}
bool ChapterCanOfferSeat() const {
    return s.chapters.version == 1 && ChapterHas(s.chapters, BeyondWorkObserved) &&
           ChapterPerformanceKnown();
}
// Only the first new observation is guaranteed. Subsequent personal actions
// still pass through the existing agency/permission rules.
bool TryChapterObservation(bool voice_busy) {
    auto& c = s.chapters;
    if (c.version != 1 || ChapterHas(c, BeyondWorkObserved) ||
        (!ChapterHas(c, DroughtEnded) && !ChapterHas(c, LegacyChapterBaseline)) ||
        !ChapterPerformanceKnown() || voice_busy || LifeActive() || LifeWorkPending() ||
        (c.observation_sequence && s.life.cooldown) || !CanInviteLife(2, 6))
        return false;
    if (!StartLifeRequest(2, 6, unsigned(LifeReason::ArchiveReflection), MemoryActor::System))
        return false;
    c.observation_sequence = s.life.sequence;
    return true;
}
