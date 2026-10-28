#pragma once
#include <lvgl.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace spring_ui {
struct Rect {
    int x, y, width, height;
};
constexpr Rect kDialog = {8, 76, 464, 364};
inline void LayoutDialog(lv_obj_t* dialog, lv_obj_t* primary, lv_obj_t* back,
                         lv_obj_t* const choices[3]) {
    lv_obj_set_pos(dialog, kDialog.x, kDialog.y);
    lv_obj_set_size(dialog, kDialog.width, kDialog.height);
    for (unsigned i = 0; i < 3; ++i) {
        lv_obj_set_pos(choices[i], 14 + 150 * i, 230);
        lv_obj_set_size(choices[i], 136, 56);
    }
    lv_obj_set_pos(primary, 14, 296);
    lv_obj_set_size(primary, 210, 56);
    lv_obj_set_pos(back, 240, 296);
    lv_obj_set_size(back, 210, 56);
}
inline void LayoutCommunicationCard(lv_obj_t* dialog, lv_obj_t* hint, lv_obj_t* back,
                                    lv_obj_t* const wish_choices[3] = nullptr) {
    lv_obj_set_pos(dialog, 34, 76);
    lv_obj_set_size(dialog, 412, wish_choices ? 260 : 190);
    lv_obj_set_pos(hint, 20, wish_choices ? 210 : 144);
    lv_obj_set_pos(back, 240, wish_choices ? 202 : 132);
    if (wish_choices)
        for (unsigned i = 0; i < 3; ++i) {
            lv_obj_set_pos(wish_choices[i], 14 + i * 128, 134);
            lv_obj_set_size(wish_choices[i], 120, 48);
        }
    lv_obj_set_size(back, 154, 46);
}
// LVGL aligns the unscaled label box; place its scaled result instead.
inline void PlaceButtonText(lv_obj_t* button, lv_obj_t* label) {
    lv_obj_update_layout(button);
    const int scale = lv_obj_get_style_transform_scale_y(label, LV_PART_MAIN);
    const auto* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const int shown_height = font->line_height * scale / 256;
    lv_obj_set_style_align(label, LV_ALIGN_TOP_LEFT, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(label, 12, (lv_obj_get_height(button) - shown_height) / 2);
}
constexpr Rect kPlots[] = {{52, 207, 106, 167}, {182, 207, 106, 167}, {312, 207, 94, 167}};
constexpr Rect kTree = {407, 192, 68, 147};
// Fixed square-screen layout. All three plots and the tree are visible at once.
constexpr int kPlantX[] = {130, 234, 338};
constexpr int kSoilY = 407, kRobotGroundY = 369, kTreeX = 451, kTreeGroundY = 346;
constexpr Rect kWorldPlots[] = {{81, 335, 98, 113}, {185, 335, 98, 113}, {289, 335, 98, 113}};
constexpr Rect kWorldTree = {395, 90, 85, 245};
constexpr Rect kResources[] = {
    {12, 12, 66, 44}, {82, 12, 66, 44}, {152, 12, 66, 44}, {222, 12, 66, 44}, {292, 12, 66, 44}};
constexpr Rect kJournal = {414, 12, 54, 54};
constexpr Rect kCommunication = {14, 372, 72, 72};
// Legacy toolbar rectangles retained only for old input regression coverage.
constexpr Rect kToolbar[] = {
    {12, 74, 102, 42}, {130, 74, 102, 42}, {248, 74, 102, 42}, {366, 74, 102, 42}};
inline unsigned NextCommunication(unsigned notices) {
    for (unsigned bit : {4u, 5u, 2u, 3u, 0u, 1u})
        if (notices & (1u << bit))
            return bit;
    return 99;
}
inline void RadioGlyph(lv_obj_t* parent) {
    auto* stem = lv_obj_create(parent);
    lv_obj_remove_style_all(stem);
    lv_obj_remove_flag(stem, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(stem, 34, 31);
    lv_obj_set_size(stem, 4, 23);
    lv_obj_set_style_radius(stem, 2, 0);
    lv_obj_set_style_bg_color(stem, lv_color_hex(0xEEF3C7), 0);
    lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);
    auto* dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(dot, 31, 23);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, 5, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xEEF3C7), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    for (int radius : {17, 26})
        for (int side = 0; side < 2; ++side) {
            auto* arc = lv_arc_create(parent);
            lv_obj_remove_style_all(arc);
            lv_obj_remove_flag(
                arc, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
            lv_obj_set_size(arc, radius * 2, radius * 2);
            lv_obj_set_pos(arc, 36 - radius, 28 - radius);
            lv_arc_set_bg_angles(arc, 0, 360);
            lv_arc_set_angles(arc, side ? 315 : 135, side ? 45 : 225);
            lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(arc, lv_color_hex(0xEEF3C7), LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
            lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
        }
}
// LVGL label/style setters invalidate even when the value is unchanged.
inline bool TextChanged(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) == 0)
        return false;
    lv_label_set_text(label, text);
    return true;
}
inline bool TextFormat(lv_obj_t* label, const char* format, ...) {
    char text[768];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return TextChanged(label, text);
}
inline void ColorChanged(lv_obj_t* object, lv_color_t color) {
    if (!lv_color_eq(lv_obj_get_style_bg_color(object, LV_PART_MAIN), color))
        lv_obj_set_style_bg_color(object, color, 0);
}
inline void OpacityChanged(lv_obj_t* object, lv_opa_t opacity) {
    if (lv_obj_get_style_bg_opa(object, LV_PART_MAIN) != opacity)
        lv_obj_set_style_bg_opa(object, opacity, 0);
}
inline lv_obj_t* Target(lv_obj_t* parent, Rect area) {
    auto* target = lv_obj_create(parent);
    lv_obj_remove_style_all(target);
    lv_obj_remove_flag(target, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(target,
                    static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_set_pos(target, area.x, area.y);
    lv_obj_set_size(target, area.width, area.height);
    return target;
}
// Keep an invitation under the finger until release, even if a short work job
// begins meanwhile. Invalidated invitations and modal/voice changes still hide it.
inline void ConfigureInvitationBubble(lv_obj_t* object) {
    lv_obj_add_flag(object,
                    static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_set_ext_click_area(object, 12);
}
inline void PlaceInvitationBubble(lv_obj_t* object, bool valid, bool idle, int x, int y) {
    if (valid && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) &&
        lv_obj_has_state(object, LV_STATE_PRESSED))
        return;
    if (valid && idle) {
        lv_obj_set_pos(object, x, y);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}
inline void Passive(lv_obj_t* object) { lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE); }
inline void DialogVisible(lv_obj_t* backdrop, lv_obj_t* dialog, bool visible) {
    if (visible) {
        lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(dialog, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(backdrop);
    } else {
        lv_obj_add_flag(dialog, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
    }
}
}  // namespace spring_ui
