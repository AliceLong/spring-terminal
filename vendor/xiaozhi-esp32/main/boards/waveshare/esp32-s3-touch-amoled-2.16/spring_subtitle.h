#pragma once
#include <lvgl.h>

namespace spring_ui {
// A top-layer label cannot inherit the active screen's downloaded CJK font.
inline void BindSubtitleFont(lv_obj_t* label, const lv_font_t* font) {
    if (font) lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
}
}  // namespace spring_ui
