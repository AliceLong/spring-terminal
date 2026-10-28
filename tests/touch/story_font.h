#pragma once
#include <cassert>
#include <fstream>
#include <lvgl.h>
#include <src/font/fmt_txt/lv_font_fmt_txt.h>
#include <vector>
// Convert 32-bit cbin pointers to host pointers; glyph bytes and device metrics
// are unchanged.
struct StoryFont {
  std::vector<uint8_t> bytes;
  std::vector<lv_font_fmt_txt_cmap_t> maps;
  lv_font_fmt_txt_dsc_t desc{};
  lv_font_fmt_txt_kern_classes_t kern{};
  lv_font_t font{};
  uint32_t u32(size_t p) const {
    assert(p + 4 <= bytes.size());
    return uint32_t(bytes[p]) | (uint32_t(bytes[p + 1]) << 8) |
           (uint32_t(bytes[p + 2]) << 16) | (uint32_t(bytes[p + 3]) << 24);
  }
  uint16_t u16(size_t p) const {
    return bytes[p] | (uint16_t(bytes[p + 1]) << 8);
  }
  explicit StoryFont(const char *path) {
    std::ifstream f(path, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(f), {});
    assert(bytes.size() > 36);
    const size_t d = u32(24);
    const auto bits = u16(d + 18);
    desc.glyph_bitmap = bytes.data() + d + u32(d);
    desc.glyph_dsc = reinterpret_cast<const lv_font_fmt_txt_glyph_dsc_t *>(
        bytes.data() + d + u32(d + 4));
    desc.kern_scale = u16(d + 16);
    desc.cmap_num = bits & 511;
    desc.bpp = (bits >> 9) & 15;
    desc.kern_classes = (bits >> 13) & 1;
    desc.bitmap_format = (bits >> 14) & 3;
    desc.stride = bytes[d + 20];
    const size_t m = d + u32(d + 8);
    maps.resize(desc.cmap_num);
    for (unsigned i = 0; i < maps.size(); ++i) {
      size_t p = m + i * 20;
      auto &a = maps[i];
      a.range_start = u32(p);
      a.range_length = u16(p + 4);
      a.glyph_id_start = u16(p + 6);
      a.unicode_list = u32(p + 8) ? reinterpret_cast<const uint16_t *>(
                                        bytes.data() + m + u32(p + 8))
                                  : nullptr;
      a.glyph_id_ofs_list =
          u32(p + 12) ? bytes.data() + m + u32(p + 12) : nullptr;
      a.list_length = u16(p + 16);
      a.type = static_cast<lv_font_fmt_txt_cmap_type_t>(bytes[p + 18]);
    }
    desc.cmaps = maps.data();
    if (u32(d + 12)) {
      assert(desc.kern_classes);
      const size_t k = d + u32(d + 12);
      kern.class_pair_values =
          reinterpret_cast<const int8_t *>(bytes.data() + k + u32(k));
      kern.left_class_mapping = bytes.data() + k + u32(k + 4);
      kern.right_class_mapping = bytes.data() + k + u32(k + 8);
      kern.left_class_cnt = bytes[k + 12];
      kern.right_class_cnt = bytes[k + 13];
      desc.kern_dsc = &kern;
    }
    font.get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt;
    font.get_glyph_bitmap = lv_font_get_bitmap_fmt_txt;
    font.line_height = u32(12);
    font.base_line = u32(16);
    font.static_bitmap = 1;
    font.dsc = &desc;
  }
};
