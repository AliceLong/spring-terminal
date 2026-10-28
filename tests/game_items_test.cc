#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>
#include "game/game_core.h"
#include "game/world_state_json.h"
using namespace desk_gap;
void Step(GameCore& g,uint64_t ms) {g.Tick(g.Snapshot().elapsed_ms+ms); assert(GameCore::Valid(g.Snapshot()));}
unsigned Count(const GameState& s,EventType type,Item target) {unsigned count=0; for(uint32_t i=0;i<s.event_count;++i) count+=s.events[i].type==type && s.events[i].target==target; return count;}
void Until(GameCore& g,Item item,ConstructionState wanted) {for(int i=0;i<1000 && ItemStage(g.Snapshot(),item)!=wanted;++i) Step(g,g.Snapshot().remaining_ms); assert(ItemStage(g.Snapshot(),item)==wanted);}
int main() {
    static_assert(sizeof(ProgressV2)==896,"Do not change V2 disk layout");
    GameCore fresh; assert(fresh.Restore({},true)); assert(Comfort(fresh.Snapshot())==10);
    assert(!fresh.Deliver(Item::kCotton)); assert(!fresh.Deliver(Item::kPaperclip)); assert(!fresh.Deliver(Item::kNone));
    assert(fresh.DeliverBottleCap()); assert(!fresh.DeliverBottleCap());
    Step(fresh,1000); assert(fresh.Snapshot().phase==CreatureState::kInspect);
    Step(fresh,4000); assert(fresh.Snapshot().resonance==10);
    Step(fresh,5000); assert(fresh.Snapshot().construction==ConstructionState::kCarried);
    Step(fresh,1000); assert(fresh.Snapshot().construction==ConstructionState::kWaiting);
    Until(fresh,Item::kBottleCap,ConstructionState::kBuilding);
    assert(fresh.Snapshot().sticks==2); Step(fresh,119999); assert(fresh.Snapshot().construction==ConstructionState::kBuilding);
    fresh.OnTouch(); assert(fresh.Snapshot().creature_state==CreatureState::kReact);
    assert(fresh.Snapshot().phase==CreatureState::kBuild && fresh.Snapshot().remaining_ms==1);
    Step(fresh,1); assert(Comfort(fresh.Snapshot())==30 && fresh.Snapshot().sticks==0);
    assert(Count(fresh.Snapshot(),EventType::kBuildCompleted,Item::kTable)==1);
    // Existing V2 progress, rewards and events migrate without a new free item or currency.
    ProgressV2 old=static_cast<const ProgressV2&>(fresh.Snapshot()); old.version=2;
    GameCore migrated; assert(migrated.MigrateV2(old,true));
    assert(migrated.Snapshot().paper==old.paper && migrated.Snapshot().resonance==old.resonance && Comfort(migrated.Snapshot())==30);
    assert(!migrated.DeliverBottleCap()); assert(migrated.Snapshot().next_event_id==old.next_event_id+1);
    // Save mid-table-build and migrate that exact old single-task layout.
    old={}; old.version=2; old.construction=ConstructionState::kBuilding; old.phase=CreatureState::kBuild; old.remaining_ms=40000; old.sticks=2; old.claimed_rewards=1;
    GameCore old_build; assert(old_build.MigrateV2(old,true)); Step(old_build,40000); assert(old_build.Snapshot().construction==ConstructionState::kCompleted);
    PersistentProgress rich; rich.resonance=40; rich.sticks=2;
    GameCore g; assert(g.Restore(rich,true)); Step(g,1234); auto search_remaining=g.Snapshot().remaining_ms;
    assert(g.Deliver(Item::kCotton)); assert(g.Snapshot().resonance==25 && g.Snapshot().suspended_search_ms==search_remaining);
    assert(g.Deliver(Item::kPaperclip)); assert(g.Snapshot().resonance==0);
    assert(g.DeliverBottleCap()); assert(!g.Deliver(Item::kCotton));
    Until(g,Item::kBottleCap,ConstructionState::kBuilding);
    assert(g.Snapshot().active_item==Item::kBottleCap); // Ready recipes take precedence over pending items.
    Step(g,120000);
    Until(g,Item::kCotton,ConstructionState::kBuilding);
    assert(g.Snapshot().remaining_ms==45000);
    Step(g,22000); auto mid=g.Snapshot();
    GameCore resumed; assert(resumed.Restore(mid,true)); assert(!resumed.Deliver(Item::kCotton));
    Step(resumed,22999); assert(resumed.Snapshot().bed==ConstructionState::kBuilding);
    Step(resumed,1); assert(resumed.Snapshot().bed==ConstructionState::kCompleted && Comfort(resumed.Snapshot())==45);
    assert(resumed.Snapshot().resonance==mid.resonance+15 && (resumed.Snapshot().claimed_rewards&8));
    Until(resumed,Item::kPaperclip,ConstructionState::kBuilding);
    Step(resumed,60000); assert(resumed.Snapshot().storage==ConstructionState::kCompleted && Comfort(resumed.Snapshot())==45);
    assert(!resumed.Deliver(Item::kPaperclip));
    // Exact same time after reboot cannot rebuild or issue a second +15 bed reward.
    auto completed=resumed.Snapshot(); GameCore after; assert(after.Restore(completed,true)); Step(after,1);
    assert(after.Snapshot().resonance==completed.resonance);
    assert(Count(after.Snapshot(),EventType::kBuildCompleted,Item::kBed)==1);
    auto revision=after.Snapshot().revision;
    std::cout << WorldStateJson(after.Snapshot(),1) << '\n' << AvailableItemsJson(after.Snapshot()) << '\n' << RecentEventsJson(after.Snapshot()) << '\n';
    assert(after.Snapshot().revision==revision);
    // Atomic charge: save and resume queued items during another build, no second charge.
    auto charged=mid; charged.last_wall_epoch=1792252800; charged.unsynced_online_ms=0;
    GameCore offline; assert(offline.Restore(charged,true)); offline.SyncClock(1792252980);
    assert(offline.Snapshot().bed==ConstructionState::kCompleted && offline.Snapshot().storage==ConstructionState::kCompleted);
    auto settled=offline.Snapshot(); offline.SyncClock(1792252980); assert(offline.Snapshot().game_ms==settled.game_ms);
    assert(offline.Snapshot().resonance==settled.resonance);
    GameCore offline_restart; assert(offline_restart.Restore(settled,true)); offline_restart.SyncClock(1792252980); assert(offline_restart.Snapshot().offline_seconds==0);
    // Source-specific real-time cooldowns and daily caps persist; demo scale cannot bypass them.
    GameCore rewards; assert(rewards.Restore({},true)); assert(!rewards.OnVoiceInteraction()); rewards.SyncClock(1792252800);
    assert(rewards.OnVoiceInteraction()); assert(!rewards.OnVoiceInteraction()); assert(rewards.OnShake()); assert(!rewards.OnShake());
    rewards.SetTimeScale(10); Step(rewards,30000); rewards.SyncClock(1792252830); assert(!rewards.OnVoiceInteraction()); assert(rewards.OnShake());
    for(int i=2;i<5;++i) {rewards.SyncClock(1792252800+30*i); assert(rewards.OnShake());}
    rewards.SyncClock(1792252950); assert(!rewards.OnShake());
    rewards.SyncClock(1792253100); assert(rewards.OnVoiceInteraction()); rewards.SyncClock(1792253400); assert(rewards.OnVoiceInteraction());
    rewards.SyncClock(1792253700); assert(!rewards.OnVoiceInteraction());
    GameCore reward_restart; assert(reward_restart.Restore(rewards.Snapshot(),true)); reward_restart.SyncClock(1792253700); assert(!reward_restart.OnVoiceInteraction() && !reward_restart.OnShake());
    reward_restart.SyncClock(1792339200); assert(reward_restart.OnVoiceInteraction()); assert(reward_restart.OnShake());
    GameCore no_storage; assert(no_storage.Restore(rich,false)); assert(!no_storage.Deliver(Item::kCotton));
    auto bad=completed; bad.bed=ConstructionState::kNotStarted; assert(!GameCore::Valid(bad)); bad=completed; bad.active_item=static_cast<Item>(99); assert(!GameCore::Valid(bad)); bad=completed; bad.carried_material=Item::kCotton; assert(!GameCore::Valid(bad));
    // A very long offline advance stays bounded and every snapshot remains restorable.
    GameCore cap; rich.last_wall_epoch=1792252800; assert(cap.Restore(rich,true)); cap.SyncClock(1792252800+86400); assert(cap.Snapshot().offline_seconds==43200 && GameCore::Valid(cap.Snapshot()));
    GameCore concurrent; assert(concurrent.Restore(rich,true)); std::atomic<bool> done=false;
    std::thread writer([&]{for(int t=0;t<1000000;t+=20) concurrent.Tick(t); done=true;});
    while(!done) {auto s=concurrent.Snapshot(); assert(GameCore::Valid(s));}
    writer.join();
    std::cerr << "Items/FSM/migration/economy/offline/concurrency tests passed\n";
}
