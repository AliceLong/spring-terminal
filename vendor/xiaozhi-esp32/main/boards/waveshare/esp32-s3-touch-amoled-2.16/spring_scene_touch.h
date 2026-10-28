#pragma once
#include "spring_art.h"
#include "spring_touch.h"
namespace spring_ui {
// Keep the hit rectangle on the visible sprite, including the moving nest.
// The rack uses the fourth planting slot instead of its hidden illustration.
inline void CreateFacilityTargets(lv_obj_t* scene, lv_obj_t* (&hits)[9], lv_event_cb_t callback) {
    for (unsigned i = 0; i < 9; ++i) {
        if (i == 4)
            continue;
        hits[i] = Target(scene, {0, 0, 1, 1});
        lv_obj_add_flag(hits[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(hits[i], callback, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(i == 8 ? 7 : i)));
    }
}
inline void SyncFacilityTargets(lv_obj_t* const (&hits)[9]) {
    for (unsigned i = 0; i < 9; ++i) {
        if (!hits[i])
            continue;
        auto* object = spring_art::FacilityObject(i);
        if (!lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_set_pos(hits[i], lv_obj_get_x(object), lv_obj_get_y(object));
            lv_obj_set_size(hits[i], lv_obj_get_width(object), lv_obj_get_height(object));
            lv_obj_remove_flag(hits[i], LV_OBJ_FLAG_HIDDEN);
        } else
            lv_obj_add_flag(hits[i], LV_OBJ_FLAG_HIDDEN);
    }
}
}  // namespace spring_ui
