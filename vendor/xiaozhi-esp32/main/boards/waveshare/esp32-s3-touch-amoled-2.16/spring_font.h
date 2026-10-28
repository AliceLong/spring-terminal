#pragma once
#include <lvgl.h>
#include <initializer_list>
namespace spring_ui {
// Reject Latin-only theme fonts, including placeholder glyphs supplied by LVGL.
inline bool HasGameChinese(const lv_font_t* font) {
    if (!font || !font->get_glyph_dsc || !font->line_height) return false;
    for (uint32_t code : {0x4E2Du, 0x6587u, 0x6C34u, 0x5EFAu, 0x8BBEu}) {
        lv_font_glyph_dsc_t glyph{};
        if (!font->get_glyph_dsc(font, &glyph, code, 0) ||
            glyph.is_placeholder || !glyph.box_w || !glyph.box_h) return false;
    }
    return true;
}
}  // namespace spring_ui
