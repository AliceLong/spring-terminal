#pragma once
#include <lvgl.h>
#include "game/spring_core.h"
#include "spring_art_data.h"
#include "spring_gardener.h"
namespace spring_art {
inline Id PlantId(spring::Plant plant, unsigned phase) {
    return static_cast<Id>(
        static_cast<unsigned>(plant == spring::Plant::Food ? Id::Plant_travelbean_seedling
                                                           : Id::Plant_tracklight_seedling) +
        (plant == spring::Plant::Food ? 0 : static_cast<unsigned>(plant) * 3) + phase);
}
inline unsigned PlotCount(const spring::Snapshot& s) {
    return s.economy.enabled && (s.economy.built & 16) ? 4 : 3;
}
inline int PlotX(const spring::Snapshot& s, unsigned i) { return i == 3 ? 446 : 130 + 104 * i; }
void Create(lv_obj_t* parent);
lv_obj_t* World();
// Index 8 is the lower animal home, sharing facility 7 with the upper nest.
lv_obj_t* FacilityObject(unsigned index);
lv_obj_t* Viewport();
void AdvanceMotion(const spring::Snapshot& state, uint64_t now_ms, bool talking,
                   spring_gardener::Voice voice = spring_gardener::Voice::None);
void GardenerActivity(uint64_t now_ms);
bool GardenerWave(uint64_t now_ms);
bool ReactShake(uint64_t now_ms, uint32_t random, spring_gardener::Voice voice,
                bool harvested = false);
const char* GardenerMode();
spring_gardener::Pose GardenerPose();
bool GardenerLifeArrived(const spring::Snapshot& state);
// Decoration can pause without stopping the robot or advancing a second game clock.
void Update(const spring::Snapshot& state, uint64_t now_ms, bool talking, bool quiet = false,
            bool speaking = false, bool animate_scenery = true);
lv_obj_t* Image(lv_obj_t* parent, Id id, int x, int y);
}  // namespace spring_art
