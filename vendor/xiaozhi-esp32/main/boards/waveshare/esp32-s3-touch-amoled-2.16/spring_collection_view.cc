#include "spring_collection_view.h"
#include <cstdio>
#include <cstring>
#include "spring_collection_assets.h"
#include "spring_touch.h"

namespace spring_collection {
void View::Create(lv_obj_t* parent, Clock source) {
    clock = source;
    layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_remove_flag(layer,
                       static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_size(layer, 480, 480);
    catcher = spring_ui::Target(layer, {0, 120, 480, 242});
    lv_obj_add_event_cb(catcher, Touch, LV_EVENT_ALL, this);
    for (auto*& image : images) {
        image = lv_image_create(layer);
        spring_ui::Passive(image);
        lv_image_set_src(image, Image(Kind::Water, 0));
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(catcher, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
}
void View::Attach(lv_obj_t* label, lv_obj_t* button) {
    status = label;
    close_button = button;
}
void View::Start() {
    enabled = true;
    session.Cancel();
    last_frame = 0;
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(layer);
    if (status)
        spring_ui::TextChanged(status, "轻晃后滑动\n练习不入库");
}
void View::Stop() {
    enabled = false;
    session.Cancel();
    lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
}
bool View::Shake(uint64_t now) {
    if (!enabled)
        return false;
    // Alternate the two visual exercises. These are not real crop growth events.
    return session.Begin(now, session.batches % 2 != 0, voice);
}
void View::Update(uint64_t now, bool call_active) {
    if (!enabled)
        return;
    if (voice != call_active) {
        session.Cancel();
        voice = call_active;
        last_frame = 0;
        lv_obj_set_pos(catcher, 0, voice ? 278 : 120);
        lv_obj_set_size(catcher, 480, voice ? 84 : 242);
    }
    if (last_frame && now - last_frame < (voice ? 100u : 50u))
        return;
    last_frame = now;
    ++frames;
    session.Tick(now);
    // Keep the exercise's band active through settlement. A late swipe must
    // not fall through into a plant/facility button after particles disappear.
    lv_obj_remove_flag(catcher, LV_OBJ_FLAG_HIDDEN);
    for (unsigned i = 0; i < images.size(); ++i) {
        const auto& p = session.particles[i];
        const bool visible = i < session.count && session.active &&
                             (p.phase == Phase::Floating || p.phase == Phase::Caught);
        if (!visible) {
            lv_obj_add_flag(images[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto* source = Image(p.kind, p.phase == Phase::Caught ? 1 : (now / 300) & 1);
        if (lv_image_get_src(images[i]) != source)
            lv_image_set_src(images[i], source);
        const int x = p.x - 16, y = p.y - 16;
        if (lv_obj_get_x(images[i]) != x || lv_obj_get_y(images[i]) != y)
            lv_obj_set_pos(images[i], x, y);
        lv_obj_remove_flag(images[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (auto* object : {status, close_button}) {
        if (!object)
            continue;
        if (voice)
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
    if (status && !voice) {
        char message[160];
        if (session.active || session.count)
            std::snprintf(message, sizeof(message), "%s %u/%u\n练习不入库",
                          session.active ? (session.growth ? "照料" : "接取") : "再试",
                          session.collected, session.count);
        else
            std::snprintf(message, sizeof(message), "轻晃后滑动\n练习不入库");
        spring_ui::TextChanged(status, message);
    }
    lv_obj_move_foreground(layer);
}
void View::Touch(lv_event_t* event) {
    auto* self = static_cast<View*>(lv_event_get_user_data(event));
    auto* input = lv_indev_active();
    if (!self || !input)
        return;
    lv_point_t point{};
    lv_indev_get_point(input, &point);
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED)
        self->session.Down(point.x, point.y);
    else if (code == LV_EVENT_PRESSING) {
        const auto now = self->clock ? self->clock() : lv_tick_get();
        if (self->session.Move(point.x, point.y, now)) {
            self->last_frame = 0;
            self->Update(now, self->voice);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
        self->session.Up();
}
Metrics View::GetMetrics() const {
    return {enabled,        session.active,         session.active ? session.count : 0,
            frames,         session.batches,        session.hits,
            session.misses, session.ignored_shakes, session.cancels,
            max_update_us};
}
}  // namespace spring_collection
