#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include "game/game_core.h"
#include "game/world_state_json.h"
using namespace desk_gap;
void Step(GameCore& g, uint64_t ms) { g.Tick(g.Snapshot().elapsed_ms + ms); }
unsigned Count(const GameState& s, EventType type) {
    unsigned n = 0;
    for (uint32_t i=0; i<s.event_count; ++i) n += s.events[i].type == type;
    return n;
}
int main() {
    GameCore g;
    assert(g.Restore({}, true));
    assert(g.DeliverBottleCap());
    assert(!g.DeliverBottleCap());
    assert(g.Snapshot().resonance == 0);
    assert(Count(g.Snapshot(), EventType::kDelivered)==1);
    Step(g, 999); assert(g.Snapshot().construction==ConstructionState::kDelivered);
    Step(g, 1); assert(g.Snapshot().phase==CreatureState::kInspect);
    Step(g, 4000); assert(g.Snapshot().phase==CreatureState::kCarry);
    assert(g.Snapshot().resonance==10);
    assert(Count(g.Snapshot(),EventType::kDiscovered)==1);
    std::cout << RecentEventsJson(g.Snapshot()) << '\n';
    Step(g, 6000); assert(g.Snapshot().construction==ConstructionState::kWaiting);
    assert(Count(g.Snapshot(),EventType::kCarried)==1);
    Step(g, 305000); assert(g.Snapshot().sticks==1);
    // Short rest/wander/sleep before the next 5-minute search.
    Step(g, 28000+305000);
    assert(g.Snapshot().phase==CreatureState::kBuild);
    assert(g.Snapshot().sticks==2);
    assert(g.Snapshot().remaining_ms==120000);
    auto building = static_cast<const PersistentProgress&>(g.Snapshot());
    GameCore restart;
    assert(restart.Restore(building,true));
    Step(restart,119999); assert(restart.Snapshot().construction==ConstructionState::kBuilding);
    Step(restart,1);
    auto built=restart.Snapshot();
    assert(built.construction==ConstructionState::kCompleted && built.sticks==0);
    assert((built.claimed_rewards&3)==3 && built.resonance==22); // 2 natural points + 20 rewards.
    assert(Count(built,EventType::kBuildCompleted)==1);
    assert(Count(built,EventType::kFurnitureUsed)==1);
    assert(!restart.DeliverBottleCap());
    std::cout << WorldStateJson(built,1) << '\n';
    GameCore after;
    assert(after.Restore(static_cast<const PersistentProgress&>(built),true));
    Step(after,1000);
    assert(after.Snapshot().resonance==built.resonance);
    assert(Count(after.Snapshot(),EventType::kBuildCompleted)==1);
    auto before=after.Snapshot();
    for(int i=0;i<100;++i) { assert(!WorldStateJson(after.Snapshot(),i).empty()); assert(!RecentEventsJson(after.Snapshot()).empty()); }
    assert(after.Snapshot().revision==before.revision);
    after.Tick(0); assert(after.Snapshot().elapsed_ms==before.elapsed_ms);
    after.OnTouch(); Step(after,20); assert(after.Snapshot().reacting);
    assert(after.Snapshot().construction==ConstructionState::kCompleted);
    auto saved=after.Snapshot().revision;
    after.MarkSaved(saved+1); assert(after.Snapshot().saved_revision<saved+1);
    after.MarkSaved(saved); after.MarkSaved(saved-1); assert(after.Snapshot().saved_revision==saved);
    // Legacy migration preserves paper and starts with no fictional cap or reward.
    GameCore legacy;
    assert(legacy.Migrate({1,64,64},true));
    assert(legacy.Snapshot().paper==64 && legacy.Snapshot().sticks==0 && legacy.Snapshot().resonance==0);
    assert(legacy.Snapshot().construction==ConstructionState::kNotStarted);
    assert(!legacy.Migrate({9,1,1},true));
    assert(!legacy.Migrate({1,1,2},true));
    // Offline catch-up uses real UTC and excludes already simulated unsynced uptime.
    PersistentProgress offline = building;
    offline.last_wall_epoch=1792252800;
    offline.unsynced_online_ms=0;
    offline.remaining_ms=60000;
    GameCore off;
    assert(off.Restore(offline,true));
    Step(off,10000);
    off.SyncClock(1792252870);
    assert(off.Snapshot().offline_seconds==60);
    assert(off.Snapshot().construction==ConstructionState::kCompleted);
    auto applied=off.Snapshot();
    off.SyncClock(1792252870);
    assert(off.Snapshot().game_ms==applied.game_ms);
    assert(Count(off.Snapshot(),EventType::kBuildCompleted)==1);
    std::cout << WorldStateJson(off.Snapshot(),2) << '\n';
    // Reboot after settlement never applies the same interval or reward again.
    GameCore off_restart;
    assert(off_restart.Restore(static_cast<const PersistentProgress&>(off.Snapshot()),true));
    off_restart.SyncClock(1792252870);
    assert(off_restart.Snapshot().offline_seconds==0);
    assert(off_restart.Snapshot().resonance==off.Snapshot().resonance);
    // Live time saved before obtaining UTC must not be counted again after another reboot.
    GameCore unanchored;
    assert(unanchored.Restore(offline,true));
    Step(unanchored,10000);
    GameCore unanchored_restart;
    assert(unanchored_restart.Restore(unanchored.Snapshot(),true));
    Step(unanchored_restart,10000);
    unanchored_restart.SyncClock(1792252870);
    assert(unanchored_restart.Snapshot().offline_seconds==50);
    GameCore cap;
    assert(cap.Restore(offline,true));
    cap.SyncClock(1792252800+86400);
    assert(cap.Snapshot().offline_seconds==43200 && cap.Snapshot().resonance<=100);
    assert(cap.Snapshot().event_count==20 && GameCore::Valid(cap.Snapshot()));
    GameCore rollback;
    assert(rollback.Restore(offline,true));
    rollback.SyncClock(1792252790);
    assert(rollback.Snapshot().clock_rollback && rollback.Snapshot().offline_seconds==0);
    assert(rollback.Snapshot().game_ms==offline.game_ms);
    GameCore unknown;
    assert(unknown.Restore({},true));
    unknown.SyncClock(0); assert(!unknown.Snapshot().clock_synced);
    unknown.SyncClock(1792252800); assert(unknown.Snapshot().offline_seconds==0);
    // Natural recovery fractional remainder survives saves.
    GameCore fractions; assert(fractions.Restore({},true)); Step(fractions,359999);
    assert(fractions.Snapshot().resonance==0 && fractions.Snapshot().resonance_fraction_ms==359999);
    GameCore fraction_restart; assert(fraction_restart.Restore(fractions.Snapshot(),true));
    Step(fraction_restart,1); assert(fraction_restart.Snapshot().resonance==1);
    PersistentProgress bad;
    bad.version=99; assert(!GameCore::Valid(bad));
    bad={}; bad.event_count=21; assert(!GameCore::Valid(bad));
    bad={}; bad.remaining_ms=0; assert(!GameCore::Valid(bad));
    bad={}; bad.phase=CreatureState::kBuild; assert(!GameCore::Valid(bad));
    GameCore no_storage; assert(no_storage.Restore({},false)); assert(!no_storage.DeliverBottleCap());
    GameCore demo; assert(demo.Restore({},true)); demo.SetTimeScale(99); assert(demo.Snapshot().time_scale==1);
    demo.SetTimeScale(10); Step(demo,30000); assert(demo.Snapshot().game_ms==300000);
    GameCore concurrent; assert(concurrent.Restore({},true));
    std::atomic<bool> done{false};
    std::thread writer([&]{ for(uint64_t t=0;t<1000000;t+=20) concurrent.Tick(t); done=true; });
    while(!done) { auto s=concurrent.Snapshot(); assert(GameCore::Valid(s)); assert(s.x>=68 && s.x<=198 && s.sprite_frame>=0 && s.sprite_frame<8); }
    writer.join();
}
