#pragma once
#include <lvgl.h>
#include <array>
#include "scene_plan.h"

namespace desk_gap {
class SceneView {
public:
    bool Start(lv_obj_t* parent);
    void Render(const GameState& state);
    lv_obj_t* TouchTarget() const { return touch_; }
    lv_obj_t* WindowTouchTarget() const { return stage_; }

private:
    ScenePlanner planner_;
    std::array<lv_obj_t*, 11> sprites_{};
    std::array<unsigned, 11> order_{};
    lv_obj_t* stage_ = nullptr;
    lv_obj_t* shadow_ = nullptr;
    lv_obj_t* touch_ = nullptr;
};
}  // namespace desk_gap
