#include "spring_art.h"
#include "spring_collection_assets.h"
#include "spring_collection_view.h"
#include "spring_touch.h"
#include "story_font.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <src/draw/lv_image_decoder_private.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <src/core/lv_global.h>

alignas(64) static uint8_t
    frame[480 * 480 * 4]; // Host capture only, NOT a device allocation.
alignas(64) static uint8_t draw_rows[480 * 40 * 4];
static uint64_t flushed_pixels = 0;
static lv_point_t point;
static bool down;
static bool rgb565;
static void Input(lv_indev_t *, lv_indev_data_t *data) {
  data->point = point;
  data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void Capture(const std::string &path) {
  std::ofstream out(path, std::ios::binary);
  out << "P6\n480 480\n255\n";
  for (unsigned i = 0; i < 480 * 480; ++i) {
    const char rgb[] = {char(frame[i * 4 + 2]), char(frame[i * 4 + 1]),
                        char(frame[i * 4])};
    out.write(rgb, 3);
  }
}
static lv_obj_t *Hit(int x, int y) {
  lv_point_t p{x, y};
  lv_obj_update_layout(lv_screen_active());
  auto *hit = lv_indev_search_obj(lv_layer_top(), &p);
  return hit ? hit : lv_indev_search_obj(lv_screen_active(), &p);
}
int main(int argc, char **argv) {
  assert(argc >= 2 && argc <= 4);
  lv_init();
  StoryFont font("vendor/xiaozhi-esp32/managed_components/78__xiaozhi-fonts/"
                 "cbin/font_noto_sans_common_30_4.bin");
  const char *words =
      "轻晃摇落滑动接取练习不额外入库照料经过物件再试一次当前内"
      "存紧张暂未开启自动养护照常将在独立演示存档中正常进度保留十倍推进经营先进"
      "入结束后可这里切回两种交替出现资源与叶片分别保存开始记录仪";
  for (unsigned i = 0; words[i];) {
    uint32_t c = static_cast<unsigned char>(words[i++]);
    if (c >= 0xe0) {
      c &= 15;
      c = (c << 6) | (words[i++] & 63);
      c = (c << 6) | (words[i++] & 63);
    }
    lv_font_glyph_dsc_t glyph{};
    assert(font.font.get_glyph_dsc(&font.font, &glyph, c, 0) &&
           !glyph.is_placeholder);
  }
  auto *display = lv_display_create(480, 480);
  rgb565 = std::getenv("COLLECTION_RGB565") != nullptr;
  lv_display_set_color_format(display, rgb565 ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_ARGB8888);
  lv_display_set_buffers(display, draw_rows, nullptr, sizeof(draw_rows),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(
      display, [](lv_display_t *d, const lv_area_t *area, uint8_t *map) {
        const auto width = area->x2 - area->x1 + 1;
        const auto height = area->y2 - area->y1 + 1;
        flushed_pixels += width * height;
        for (int row = 0; row < height; ++row) {
          auto *dest = frame + ((area->y1 + row) * 480 + area->x1) * 4;
          if (!rgb565)
            std::memcpy(dest, map + row * width * 4, width * 4);
          else
            for (int col = 0; col < width; ++col) {
              const unsigned off = (row * width + col) * 2;
              const uint16_t pixel = map[off] | (map[off + 1] << 8);
              dest[col * 4] = (pixel & 31) * 255 / 31;
              dest[col * 4 + 1] = ((pixel >> 5) & 63) * 255 / 63;
              dest[col * 4 + 2] = ((pixel >> 11) & 31) * 255 / 31;
              dest[col * 4 + 3] = 255;
            }
        }
        lv_display_flush_ready(d);
      });
  lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);
  auto *stage = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(stage);
  lv_obj_set_size(stage, 480, 480);
  spring_ui::Passive(stage);
  if (argc == 2)
    spring_art::Create(stage);
  spring::Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.Wake();
  for (auto &p : c.s.plots)
    p.ready = true;
  if (argc == 2)
    spring_art::Update(c.Get(), 1000, false);
  const auto saved = c.PackFull();
  auto *radio = spring_ui::Target(stage, spring_ui::kCommunication);
  auto *journal = spring_ui::Target(stage, spring_ui::kJournal);
  // Match the real top-layer modal shield: collecting beneath a call card must
  // still receive touch, while its close control stays above the play area.
  auto *shield = spring_ui::Target(lv_layer_top(), {0, 0, 480, 480});
  auto *card = spring_ui::Target(shield, {34, 76, 412, 190});
  lv_obj_set_style_bg_color(card, lv_color_hex(0xF2ECD8), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  auto *call_close = spring_ui::Target(card, {240, 132, 154, 46});
  lv_obj_add_flag(shield, LV_OBJ_FLAG_HIDDEN);
  lv_refr_now(display);
  lv_mem_monitor_t before{}, warm{}, after{};
  lv_mem_monitor(&before);
  // Host-only scope: discard earlier scene/font peaks so they cannot hide
  // the exercise's transient allocations in a historical high-water counter.
  auto &allocator = LV_GLOBAL_DEFAULT()->tlsf_state;
  const auto baseline_used = allocator.cur_used;
  allocator.max_used = baseline_used;
  spring_collection::View view;
  view.Create(lv_layer_top());
  auto *label = lv_label_create(view.Layer());
  spring_ui::Passive(label);
  lv_obj_set_style_text_font(label, &font.font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xFFF3D6), 0);
  const int scale = 256;
  lv_obj_set_style_transform_scale_x(label, scale, 0);
  lv_obj_set_style_transform_scale_y(label, scale, 0);
  lv_obj_set_style_transform_pivot_x(label, 0, 0);
  lv_obj_set_style_transform_pivot_y(label, 0, 0);
  lv_obj_set_pos(label, 90, 75);
  lv_obj_set_width(label, 294 * 256 / scale);
  lv_obj_set_style_bg_color(label, lv_color_hex(0x263C32), 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_90, 0);
  auto *close = spring_ui::Target(view.Layer(), {396, 76, 72, 44});
  lv_obj_set_style_bg_color(close, lv_color_hex(0x455E50), 0);
  lv_obj_set_style_bg_opa(close, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(close, 10, 0);
  auto *close_text = lv_label_create(close);
  spring_ui::Passive(close_text);
  lv_label_set_text(close_text, "返回");
  lv_obj_set_style_text_font(close_text, &font.font, 0);
  lv_obj_set_style_text_color(close_text, lv_color_hex(0xFFF3D6), 0);
  const int button_scale = 256;
  lv_obj_set_style_transform_scale_x(close_text, button_scale, 0);
  lv_obj_set_style_transform_scale_y(close_text, button_scale, 0);
  lv_obj_set_width(close_text, 60);
  spring_ui::PlaceButtonText(close, close_text);
  view.Attach(argc == 4 ? nullptr : label, close);
  auto *input = lv_indev_create();
  lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(input, Input);
  lv_indev_set_display(input, display);
  unsigned direct_aliases = 0;
  for (unsigned kind = 0; kind < 4; ++kind)
    for (unsigned f = 0; f < 2; ++f) {
      const auto *image = spring_collection::Image(
          static_cast<spring_collection::Kind>(kind), f);
      assert(image->data_size == 3072);
      lv_image_decoder_dsc_t decoded{};
      assert(lv_image_decoder_open(&decoded, image, nullptr) == LV_RESULT_OK);
      if (decoded.decoded && decoded.decoded->data == image->data)
        ++direct_aliases;
      lv_image_decoder_close(&decoded);
      lv_image_cache_drop(image);
    }
  view.Start();
  lv_tick_inc(1000);
  assert(view.Shake(lv_tick_get()));
  lv_tick_inc(500);
  view.Update(lv_tick_get(), false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/mature.ppm");
  assert(Hit(38, 400) == radio && Hit(440, 40) == journal &&
         Hit(430, 95) == close);
  point = {2, 240};
  down = true;
  lv_indev_read(input);
  point = {477, 240};
  lv_tick_inc(20);
  lv_indev_read(input);
  assert(view.State().collected == 6);
  down = false;
  lv_indev_read(input);
  lv_tick_inc(400);
  view.Update(lv_tick_get(), false);
  lv_refr_now(display);
  assert(!view.State().active);
  // Warm both frame variants and labels before leak comparison.
  for (unsigned i = 0; i < 3; ++i) {
    lv_tick_inc(7000);
    view.Shake(lv_tick_get());
    for (unsigned t = 0; t <= 6500; t += 50) {
      lv_tick_inc(50);
      view.Update(lv_tick_get(), false);
      lv_refr_now(display);
    }
  }
  lv_mem_monitor(&warm);
  uint64_t max_host_update_us = 0;
  uint64_t max_steady_pixels = 0;
  lv_obj_remove_flag(shield, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(shield);
  for (unsigned batch = 0; batch < 30; ++batch) {
    lv_tick_inc(7000);
    view.Update(lv_tick_get(), true);
    assert(view.Shake(lv_tick_get()));
    for (unsigned f = 0; f < 65; ++f) {
      lv_tick_inc(100);
      const auto start = std::chrono::steady_clock::now();
      view.Update(lv_tick_get(), true);
      const uint64_t cost =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - start)
              .count();
      max_host_update_us = std::max(max_host_update_us, cost);
      flushed_pixels = 0;
      lv_refr_now(display);
      if (f >= 2 && f < 58)
        max_steady_pixels = std::max(max_steady_pixels, flushed_pixels);
      assert(Hit(38, 400) == shield && Hit(300, 230) == call_close);
      if (batch == 0 && f == 4) {
        point = {90, 312};
        down = true;
        lv_indev_read(input);
        point = {390, 312};
        lv_tick_inc(20);
        lv_indev_read(input);
        assert(view.State().collected == view.State().count);
        down = false;
        lv_indev_read(input);
      }
    }
  }
  lv_obj_add_flag(shield, LV_OBJ_FLAG_HIDDEN);
  view.Update(lv_tick_get(), false);
  lv_tick_inc(7000);
  assert(view.Shake(lv_tick_get()));
  lv_tick_inc(600);
  view.Update(lv_tick_get(), false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/growth.ppm");
  view.Stop();
  lv_refr_now(display);
  lv_mem_monitor(&after);
  assert(warm.free_size <= after.free_size + 512);
  assert(before.free_size - after.free_size < 32 * 1024);
  assert(max_steady_pixels < 480 * 320);
  assert(c.PackFull() ==
         saved); // Presentation did not mint resources/change save.
  std::cout
      << "{\"host_only\":true,\"pixel_flash_bytes\":"
      << spring_collection::kPixelBytes
      << ",\"lvgl_retained_bytes\":" << before.free_size - after.free_size
      << ",\"additional_peak_bytes\":" << after.max_used - baseline_used
      << ",\"output_rgb565\":" << (rgb565 ? "true" : "false")
      << ",\"isolated_without_scene\":" << (argc >= 3 ? "true" : "false")
      << ",\"steady_delta_bytes\":"
      << int(warm.free_size) - int(after.free_size)
      << ",\"native_decoder_direct_aliases\":" << direct_aliases
      << ",\"host_update_max_us\":" << max_host_update_us
      << ",\"max_steady_flushed_pixels\":" << max_steady_pixels
      << ",\"device_fps_measured\":false,\"resource_save_unchanged\":true}\n";
}
