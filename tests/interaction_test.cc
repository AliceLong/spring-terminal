#include <cassert>
#include <cmath>
#include <iostream>
#include "game/pickup_detector.h"
#include "game/game_core.h"
#include "boards/waveshare/esp32-s3-touch-amoled-2.16/journal_view.h"
using namespace desk_gap;
int main() {
    PickupDetector pickup;
    for(unsigned t=0;t<4000;t+=20) assert(!pickup.Sample(t,0.005f*std::sin(t),0,0.93));
    assert(!pickup.Sample(4000,0.2,0,0.9)); assert(!pickup.Sample(4020,0.25,0,0.87)); assert(pickup.Sample(4040,0.3,0,0.85));
    // Holding still or moving inside the re-arm interval does not repeatedly wake.
    for(unsigned t=4060;t<8500;t+=20) assert(!pickup.Sample(t,0.3,0,0.85));
    assert(!pickup.Sample(8500,0.6,0,0.7)); assert(!pickup.Sample(8520,0.7,0,0.6)); assert(!pickup.Sample(8540,0.8,0,0.5));
    for(unsigned t=8560;t<10000;t+=20) assert(!pickup.Sample(t,0.8,0,0.5));
    assert(!pickup.Sample(10000,0.4,0,0.8)); assert(!pickup.Sample(10020,0.3,0,0.85)); assert(pickup.Sample(10040,0.2,0,0.9));
    assert(!pickup.Sample(20000,0,0,1)); // sample gap starts a new settling interval
    GameCore game; assert(game.Restore({},true)); auto r=game.CompleteInteraction(true); assert(r.status==RewardStatus::kClockUnavailable && !r.amount);
    game.SyncClock(1792252800); r=game.CompleteInteraction(true); assert(r.status==RewardStatus::kGranted && r.amount==2);
    r=game.CompleteInteraction(true); assert(r.status==RewardStatus::kCooldown && r.cooldown_seconds==30 && !r.amount);
    game.SyncClock(1792252829); r=game.CompleteInteraction(true); assert(r.status==RewardStatus::kCooldown && r.cooldown_seconds==1);
    game.SyncClock(1792252830); assert(game.CompleteInteraction(true).amount==2);
    for(unsigned i=2;i<5;++i) {game.SyncClock(1792252800+i*30); assert(game.CompleteInteraction(true).amount==2);}
    game.SyncClock(1792253100); assert(game.CompleteInteraction(true).status==RewardStatus::kDailyLimit);
    PersistentProgress full; full.resonance=100; GameCore capped; assert(capped.Restore(full,true)); capped.SyncClock(1792252800); assert(capped.CompleteInteraction(false).status==RewardStatus::kFull);
    GameCore missing; assert(missing.Restore({},false)); assert(missing.CompleteInteraction(true).status==RewardStatus::kStorageUnavailable);
    auto world=game.Snapshot(); assert(JournalText(world,0,0).find("简陋的住所")!=std::string::npos); assert(JournalPages(world,1)==3);
    auto revision=world.revision;
    for(unsigned i=0;i<world.event_count;++i) assert(!JournalText(world,2,i).empty());
    assert(game.Snapshot().revision==revision);
    world.event_count=1; world.events[0].timestamp=0; assert(JournalText(world,2,0).find("时间未知")!=std::string::npos);
    world.events[0].timestamp=1792252800; assert(JournalText(world,2,0).find("时间未知")==std::string::npos);
    world.events[0].offline=1; assert(JournalText(world,2,0).find("离线")!=std::string::npos);
    world.event_count=0; assert(JournalPages(world,2)==1 && JournalText(world,2,0).find("还没有记录")!=std::string::npos);
    world.bed=world.construction=world.storage=ConstructionState::kCompleted;
    assert(JournalText(world,0,0).find("45/45")!=std::string::npos);
    assert(JournalText(world,0,0).find("3/3")!=std::string::npos);
    assert(JournalText(world,1,1).find("柔软床铺")!=std::string::npos);
    assert(JournalText(world,1,2).find("不增加舒适")!=std::string::npos);
    std::cout<<"Pickup settling/rearm, reward reasons/boundaries and journal pagination passed\n";
}
