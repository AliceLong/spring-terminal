#include "scene_view.h"

#include <algorithm>
#include "scene_art.h"

namespace desk_gap {
bool SceneView::Start(lv_obj_t* parent) {
    if (!InitializeSceneArt())
        return false;
    stage_ = lv_obj_create(parent);
    lv_obj_remove_style_all(stage_);
    lv_obj_remove_flag(stage_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(stage_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(stage_, 480, 480);
    auto* background = lv_image_create(stage_);
    lv_obj_remove_flag(background, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(background, SceneBackground());
    shadow_ = lv_obj_create(stage_);
    lv_obj_remove_style_all(shadow_);
    lv_obj_remove_flag(shadow_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(shadow_, 52, 10);
    lv_obj_set_style_radius(shadow_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(shadow_, lv_color_hex(0x633B25), 0);
    lv_obj_set_style_bg_opa(shadow_, LV_OPA_30, 0);
    for (unsigned i = 0; i < sprites_.size(); ++i) {
        sprites_[i] = lv_image_create(stage_);
        lv_obj_remove_flag(sprites_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_image_set_pivot(sprites_[i], 0, 0);
        lv_image_set_scale(sprites_[i], 512);
        lv_image_set_antialias(sprites_[i], false);
        lv_obj_add_flag(sprites_[i], LV_OBJ_FLAG_HIDDEN);
        order_[i] = i;
    }
    // A separate full-size target keeps touch reliable with 2x image scaling.
    touch_ = lv_obj_create(stage_);
    lv_obj_remove_style_all(touch_);
    lv_obj_remove_flag(touch_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(touch_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(touch_, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(touch_, 96, 112);
    return true;
}

void SceneView::Render(const GameState& state) {
    if (!stage_)
        return;
    const auto plan = planner_.Update(state);
    for (unsigned i = 0; i < sprites_.size(); ++i) {
        const auto& sprite = plan.sprites[i];
        auto* image = sprites_[i];
        if (!sprite.visible) {
            lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto* source = SceneImage(sprite.art, sprite.frame, sprite.mirror);
        if (lv_image_get_src(image) != source)
            lv_image_set_src(image, source);
        lv_obj_set_pos(image, sprite.x * 2 - source->header.w,
                       sprite.y * 2 - (source->header.h - 2) * 2);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_HIDDEN);
    }
    auto sorted = order_;
    std::stable_sort(sorted.begin(), sorted.end(), [&](unsigned a, unsigned b) {
        return plan.sprites[a].depth < plan.sprites[b].depth;
    });
    if (sorted != order_) {
        // Reorder only on a crossing, never raise the scene above HUD/chat/modals.
        for (auto index : sorted)
            lv_obj_move_foreground(sprites_[index]);
        lv_obj_move_foreground(touch_);
        order_ = sorted;
    }
    lv_obj_set_pos(shadow_, state.x * 2 - 26, state.y * 2 - 5);
    lv_obj_set_pos(touch_, state.x * 2 - 48, state.y * 2 - 108);
}
}  // namespace desk_gap
