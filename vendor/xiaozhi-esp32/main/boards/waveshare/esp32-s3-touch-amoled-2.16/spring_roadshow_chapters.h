#pragma once
#include "game/spring_core.h"
namespace spring_show {
// Scene prerequisites are explicitly fixtures; requests, approvals and action results
// after loading are never prepopulated. Formal saves never pass through this function.
inline bool PrepareChapters(spring::Core& out, unsigned scene, uint32_t hi, uint32_t lo) {
    if (scene < 8 || scene > 11 || !(hi | lo))
        return false;
    out = {};
    out.storage_ok = true;
    out.demo = out.roadshow = true;
    out.roadshow_scene = scene;
    out.EnableEconomy(true);
    out.EnableStory();
    out.s.story_stage = 1;
    out.s.water = 9;  // Fixture inventory; production/capacity/time constants are untouched.
    out.s.energy = 12;
    out.s.economy.paid_mask = 7;
    for (auto& p : out.s.plots)
        p.growth_ms = 200000;
    auto& n = out.s.story;
    auto& c = out.s.chapters;
    n.completed = (1u << 0) | (1u << 1);
    n.available = n.completed;
    n.archives = 1;
    n.dry_seen = 1;
    c.flags = spring::ProductionRecorded | spring::DroughtStarted;
    c.start_water = out.s.water;
    c.start_energy = out.s.energy;
    out.s.economy.region = scene == 8 ? 1 : 2;
    if (scene >= 9) {
        c.flags |= spring::DroughtEnded;
        c.end_water = out.s.water;
        c.end_energy = out.s.energy;
        n.available |= (1u << 2) | (1u << 3) | (1u << 4);
        n.archives |= 2;
    }
    if (scene >= 10)
        n.completed |=
            (1u << 2) | (1u << 4);  // Character baseline knowledge, not a player's read receipt.
    if (scene == 11) {
        n.available |= (1u << 3) | (1u << 5);
        n.completed |= (1u << 3) | (1u << 5);
        c.flags |= spring::BeyondWorkObserved;
    }  // Baseline, with no fabricated evidence id.
    out.EnableMemory(hi, lo);
    out.TickStory(0);
    return true;
}
}  // namespace spring_show
