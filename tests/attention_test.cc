#include <cassert>
#include <iostream>
#include "game/attention_gate.h"
#include "game/game_core.h"
using namespace desk_gap;
int main() {
    AttentionGate gate;
    assert(!gate.TryShake(1000,false));
    assert(gate.TryShake(1000,true));
    assert(!gate.TryShake(30999,true));
    assert(!gate.TryShake(31000,false));
    assert(gate.TryShake(31000,true));
    assert(!gate.TryShake(100,true));
    GameCore g;
    assert(g.Restore({},true));
    g.Tick(1000);
    assert(!g.OnTouch(false));
    assert(!g.Snapshot().reacting);
    g.Tick(1200);
    auto before=g.Snapshot();
    assert(g.OnTouch(false));
    auto after=g.Snapshot();
    assert(after.reacting && after.phase==before.phase && after.remaining_ms==before.remaining_ms);
    assert(after.events[after.event_count-1].type==EventType::kKnock);
    assert(after.resonance==before.resonance);
    g.Tick(1240);assert(!g.OnTouch(false));
    assert(g.Snapshot().event_count==after.event_count);
    g.Tick(3000);assert(!g.Snapshot().reacting);
    assert(!g.OnTouch(false));
    g.Tick(3600);assert(!g.OnTouch(false));
    assert(g.Snapshot().event_count==after.event_count);
    assert(!g.OnTouch());assert(g.Snapshot().reacting);
    std::cout<<"Attention: scene double tap, debounce, progress preservation, shake busy/cooldown passed\n";
}
