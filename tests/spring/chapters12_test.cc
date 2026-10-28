#include "game/spring_json.h"
#include "spring_gardener.h"
#include "spring_roadshow.h"
#include <cassert>
#include <cstdio>
using namespace spring;
Core Fresh() {
    Core c;
    c.storage_ok = true;
    c.EnableEconomy(true);
    c.EnableMemory(57, 12);
    c.EnableStory();
    c.s.story_stage = 1;
    c.s.story.available = c.s.story.completed = 3; // Starting chapter1 after the prologue.
    c.s.water = 9;
    c.s.energy = 12;
    c.s.economy.paid_mask = 7;
    for (auto& p : c.s.plots) { p.ready = false; p.growth_ms = 200000; }
    return c;
}
void Drive(Core& c, spring_gardener::Controller& body, uint64_t& now, unsigned ms,
           bool call = false) {
    for (unsigned t = 0; t < ms; t += 100) {
        now += 100;
        c.Step(100);
        c.StepPlantWork(100, spring_gardener::PlantWorkArrived(c.Get(), body.Get()));
        c.StepLife(100, call, spring_gardener::LifeArrived(c.Get(), body.Get()), false);
        body.Step(c.Get(), now, call ? spring_gardener::Voice::Listen : spring_gardener::Voice::None);
    }
}
int main() {
    static_assert(sizeof(FullSave) == 1536);
    static_assert(kRegionYields[0][0] == 5 && kRegionYields[0][1] == 6);
    static_assert(kRegionYields[1][0] == 6 && kRegionYields[1][1] == 4);
    auto c = Fresh();
    c.s.plots[1].ready = true;
    c.s.plots[1].growth_ms = 600000;
    c.s.economy.mature_yield[1] = 6;
    assert(c.EconomyHarvest(1, false)); // Establish real production, not a fabricated milestone.
    const auto initial = c.Get();
    assert(!ChapterBench(c.s.chapters));
    assert(WorldJson(c.Get()).find("\"robot_use_unlocked\":false") != std::string::npos);
    // The existing real region boundary starts/finishes the summary; no changed timers.
    c.s.economy.region_ms = 1799000;
    c.Advance(1000);
    c.TickStory(0);
    assert(c.s.economy.region == 1 && ChapterHas(c.s.chapters, DroughtStarted));
    assert(!ChapterHas(c.s.chapters, DroughtEnded));
    assert(!(c.s.story.archives & 2));
    spring_gardener::Controller body;
    uint64_t now = 0;
    assert(c.RequestPlantWork(1, 1, 0, c.s.plots[0].cycle));
    assert(!c.s.chapters.requested_cares);
    Drive(c, body, now, 20000, true);
    assert(c.s.plant_action.status == PlantActionStatus::Completed);
    assert(c.s.chapters.requested_cares == 1);
    const auto water = c.s.water;
    assert(water == initial.water); // No new care debit or growth bonus.
    c.s.economy.region_ms = 1799000;
    c.Advance(1000);
    c.TickStory(0);
    assert(ChapterHas(c.s.chapters, DroughtEnded));
    assert(c.s.story.archives & 2);
    assert(!ChapterHas(c.s.chapters, BeyondWorkObserved));
    assert(!c.TryChapterObservation(false)); // Unread archive cannot change cognition.
    assert(c.StoryArchiveSeen(1));
    // Existing short watering can be finishing; the chapter waits for its safe point.
    Drive(c, body, now, 12000);
    assert(c.s.life.target == 6 || ChapterHas(c.s.chapters, BeyondWorkObserved));
    Drive(c, body, now, 30000);
    assert(ChapterHas(c.s.chapters, BeyondWorkObserved));
    assert(c.s.chapters.observation_evidence);
    for (unsigned wait = 0; wait < 20000 && c.s.story.pending != unsigned(StoryNode::Rest); wait += 100)
        Drive(c, body, now, 100);
    assert(c.s.story.pending == unsigned(StoryNode::Rest)); // No fixed conversation completion required.
    const auto observation_id = c.s.chapters.observation_evidence;
    assert(!c.TryChapterObservation(false)); // Exactly once after physical completion.
    assert(!ChapterBench(c.s.chapters));
    assert(c.AcceptStory(unsigned(StoryNode::Rest)));
    assert(!c.CompleteStory(unsigned(StoryNode::Rest))); // Reading/talking isn't sitting.
    assert(c.InviteLife(3));
    assert(c.s.life.under_tree == 2 && ChapterBench(c.s.chapters));
    assert(!ChapterHas(c.s.chapters, BenchRestCompleted));
    Drive(c, body, now, 40000, true);
    assert(ChapterHas(c.s.chapters, BenchRestCompleted));
    assert(c.s.chapters.bench_evidence && c.s.chapters.invitation_evidence);
    assert(c.StoryDone(StoryNode::Rest) && c.s.story.language == 1);
    assert(c.s.life.records[c.s.life.count - 1].under_tree == 2);
    assert(c.s.story.archives & 4); // Return record follows actual first seat rest.
    // Restart retains evidence and permission, never reruns the guaranteed observation.
    auto save = c.PackStory();
    Core restored;
    restored.storage_ok = true;
    assert(restored.RestoreStory(*save));
    assert(restored.s.chapters.observation_evidence == observation_id);
    assert(restored.PackStory()->checksum == save->checksum);
    auto cp = c.PackStoryCheckpoint();
    assert(restored.RestoreStoryCheckpoint(*cp));
    assert(!restored.TryChapterObservation(false));
    // Cancelled and restarted attempts cannot fabricate completed seat rest.
    auto cancelled = Fresh();
    cancelled.s.story.completed = cancelled.s.story.available = 1u << 4;
    cancelled.s.chapters.flags = BeyondWorkObserved;
    assert(cancelled.InviteLife(3));
    assert(cancelled.CancelGardenerActivity(0, cancelled.s.life.sequence));
    assert(!ChapterHas(cancelled.s.chapters, BenchRestCompleted));
    assert(cancelled.InviteLife(3));
    auto pending = cancelled.PackStory();
    Core reboot; reboot.storage_ok = true;
    assert(reboot.RestoreStory(*pending));
    reboot.StepLife(100, false, false, false);
    assert(!ChapterHas(reboot.s.chapters, BenchRestCompleted));
    // No player participation is inferred for a fully automatic drought.
    auto automatic = Fresh();
    automatic.s.economy.region_ms = 1799000;
    automatic.Advance(1000);
    automatic.Advance(1800000);
    assert(automatic.s.chapters.manual_harvests == 0 && automatic.s.chapters.requested_cares == 0);
    assert(automatic.s.chapters.automatic_harvests > 0);
    assert(ChapterHas(automatic.s.chapters, DroughtEnded));
    automatic.TickStory(0);
    assert(automatic.s.story.archives & 2); // No required player speech or manual harvest.
    // Old saves have zero extension words, not invented resource or player history.
    auto legacy = Fresh();
    legacy.s.story.completed = legacy.s.story.available = (1u << 4) | (1u << 6);
    auto old = *legacy.PackStory();
    for (unsigned i = 326; i < 346; ++i) old.core[i] = 0;
    old.core.back() = Core::FullChecksum(old.core);
    old.checksum = Core::StoryChecksum(old);
    assert(reboot.RestoreStory(old));
    assert(reboot.s.water == legacy.s.water && !reboot.s.chapters.drought_chain);
    assert(!ChapterBench(reboot.s.chapters)); // Old floor rest doesn't unlock the chair.
    auto partial = Fresh();
    partial.s.story.available |= 1u << 6;
    partial.s.story.active = 6;
    auto partial_save = *partial.PackStory();
    for (unsigned i = 326; i < 346; ++i) partial_save.core[i] = 0;
    partial_save.core.back() = Core::FullChecksum(partial_save.core);
    partial_save.checksum = Core::StoryChecksum(partial_save);
    assert(reboot.RestoreStory(partial_save));
    assert(reboot.CompleteStory(6)); // Existing ground-rest discussion must remain finishable.
    assert(!ChapterBench(reboot.s.chapters));
    // New roadshow starting facts never synthesize a player's completed actions.
    for (unsigned scene = 8; scene <= 11; ++scene) {
        Core show;
        assert(spring_show::Prepare(show, scene, 57, scene));
        assert(show.roadshow && show.s.chapters.version == 1);
        assert(!show.s.chapters.requested_cares && !show.s.chapters.manual_harvests);
        assert(!show.s.chapters.bench_evidence && !ChapterBench(show.s.chapters));
        assert(show.s.story.archives_read == 0);
        const auto furniture_json = WorldJson(show.Get());
        assert(furniture_json.find("chapter2_permission_pending") == std::string::npos);
        assert(furniture_json.find(show.ChapterCanOfferSeat()
                                       ? "\"first_invitation_ready\":true"
                                       : "\"first_invitation_ready\":false") != std::string::npos);
        auto before = show.s.game_ms;
        show.Step(100);
        assert(show.s.game_ms == before + 100); // Existing roadshow time semantics, unchanged.
        Core disk; disk.storage_ok = true; disk.demo = true;
        assert(disk.RestoreStory(*show.PackStory()));
    }
    // "Not yet invited" is not a prerequisite failure. The explicit invitation
    // itself grants first-use permission, even while a short watering finishes.
    Core seat;
    assert(spring_show::Prepare(seat, 11, 57, 99));
    assert(!ChapterBench(seat.s.chapters) && seat.ChapterCanOfferSeat());
    seat.work = Work::Water;
    seat.s.work_ms = 100;
    assert(seat.CanInviteLife(3) && seat.InviteLife(3));
    assert(ChapterBench(seat.s.chapters));
    assert(!ChapterHas(seat.s.chapters, BenchRestCompleted));
    assert(!seat.s.life.reached && seat.s.life.under_tree == 2);
    assert(spring_show::Prepare(seat, 11, 57, 100));
    seat.s.water = 5;
    assert(!seat.InviteLife(3) && !ChapterBench(seat.s.chapters));
    std::printf("Chapters12: actual drought/care, archive gate, head observation, seat permission/arrival/completion, cancellation/restart, legacy save and four isolated fixtures passed; ChapterState=%zu StorySave=%zu\n", sizeof(ChapterState), sizeof(StorySave));
}
