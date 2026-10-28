#pragma once
#include <lvgl.h>
#include "game/spring_collection.h"

namespace spring_collection {
struct Metrics {
    unsigned enabled = 0, active = 0, particles = 0, frames = 0, batches = 0;
    unsigned hits = 0, misses = 0, ignored_shakes = 0, cancels = 0, max_update_us = 0;
};
// One persistent pool, no LVGL animations/timers, no per-particle allocation.
// All methods must be called from the existing GUI context.
class View {
public:
    using Clock = uint64_t (*)();
    void Create(lv_obj_t* parent, Clock clock = nullptr);
    lv_obj_t* Layer() const { return layer; }
    void Attach(lv_obj_t* status, lv_obj_t* close_button);
    void Start();
    void Stop();
    bool Enabled() const { return enabled; }
    bool Shake(uint64_t now);
    void Update(uint64_t now, bool voice);
    Metrics GetMetrics() const;
    void RecordUpdateUs(unsigned us) { max_update_us = std::max(max_update_us, us); }
    const Session& State() const { return session; }

private:
    static void Touch(lv_event_t* event);
    lv_obj_t *layer = nullptr, *catcher = nullptr, *status = nullptr, *close_button = nullptr;
    std::array<lv_obj_t*, kCapacity> images{};
    Session session;
    bool enabled = false, voice = false;
    uint64_t last_frame = 0;
    unsigned frames = 0, max_update_us = 0;
    Clock clock = nullptr;
};
}  // namespace spring_collection
