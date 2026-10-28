#include <cassert>
#include <cmath>
#include <iostream>
#include "boards/waveshare/esp32-s3-touch-amoled-2.16/scene_plan.h"

using namespace desk_gap;
int main() {
    PersistentProgress built;
    built.construction = built.bed = built.storage = ConstructionState::kCompleted;
    built.claimed_rewards = 63;
    GameCore g;
    assert(g.Restore(built, true));
    int min_x = 240, max_x = 0, moving_ticks = 0;
    auto previous = g.Snapshot();
    ScenePlanner planner;
    for (unsigned ms = 20; ms <= 120000; ms += 20) {
        g.Tick(ms);
        auto s = g.Snapshot();
        assert(std::abs(s.x-previous.x) <= 1 && std::abs(s.y-previous.y) <= 1);
        min_x = std::min(min_x, s.x);
        max_x = std::max(max_x, s.x);
        moving_ticks += s.visual_walking;
        assert(s.construction==built.construction && s.bed==built.bed && s.storage==built.storage);
        assert(s.paper==0 && s.sticks==0 && s.claimed_rewards==63);
        auto plan = planner.Update(s);
        assert(plan.sprites[2].visible && plan.sprites[3].visible && plan.sprites[4].visible);
        assert(!plan.sprites[8].visible); // No replay of saved completion effects.
        previous=s;
    }
    assert(min_x<=70 && max_x>=192 && moving_ticks>5500);
    // Switching goals and repeated touch cannot teleport or block simulation.
    g.OnTouch();
    g.Tick(120020);
    assert(g.Snapshot().simulation_ticks==6001);
    assert(std::abs(g.Snapshot().x-previous.x)<=1);
    assert(g.Snapshot().reacting);

    GameCore delivery;
    assert(delivery.Restore({},true));
    ScenePlanner live;
    live.Update(delivery.Snapshot());
    assert(delivery.DeliverBottleCap());
    auto drop=live.Update(delivery.Snapshot());
    assert(drop.sprites[5].visible && drop.sprites[5].y==90);
    assert(drop.sprites[8].art==SCENE_ART_DELIVERY_PORTAL && drop.sprites[8].visible);
    delivery.Tick(700);
    auto landed=live.Update(delivery.Snapshot());
    assert(landed.sprites[5].y==176);
    for(unsigned ms=720;ms<=6000;ms+=20)delivery.Tick(ms);
    auto carry=live.Update(delivery.Snapshot());
    assert(delivery.Snapshot().phase==CreatureState::kCarry);
    assert(carry.sprites[5].depth==carry.sprites[0].depth+1);
    assert(std::abs(carry.sprites[5].x-carry.sprites[0].x)==13);
    // A persisted delivered item is present, but not dropped again on reboot.
    ScenePlanner reboot;
    auto saved=delivery.Snapshot();
    saved.phase=CreatureState::kIdle;saved.construction=ConstructionState::kDelivered;
    assert(reboot.Update(saved).sprites[5].y==176);

    GameState sleeper;
    sleeper.bed=ConstructionState::kCompleted;
    sleeper.phase=CreatureState::kSleep;
    sleeper.x=68;sleeper.y=146;sleeper.visual_walking=false;
    ScenePlanner bed;
    bed.Update(sleeper);
    sleeper.elapsed_ms=2400;
    auto sleep=bed.Update(sleeper);
    assert(sleep.sprites[0].art==SCENE_ART_SLEEP && sleep.sprites[0].frame>=2);
    assert(sleep.sprites[3].depth<sleep.sprites[0].depth);
    assert(sleep.sprites[10].visible && sleep.sprites[10].depth>sleep.sprites[0].depth);
    // Construction frames are derived from real progress, not a cyclic counter.
    GameState crafting;
    crafting.phase=CreatureState::kBuild;crafting.active_item=Item::kPaperclip;
    crafting.storage=ConstructionState::kBuilding;crafting.remaining_ms=1000;
    ScenePlanner builder;
    assert(builder.Update(crafting).sprites[4].frame==2);
    std::cout<<"Scene V2: continuous furniture crossings, saves/rewards, live drop, carried props, sleep layers passed\n";
}
