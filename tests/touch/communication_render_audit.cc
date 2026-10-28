#include "spring_art.h"
#include "spring_scenery.h"
#include "spring_touch.h"
#include "story_font.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <src/core/lv_global.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <string>
#include <vector>

// Host-only diagnosis. Match the device's RGB565 partial draw and 2MiB cache.
alignas(64) static uint8_t draw_rows[480 * 40 * 2];
static uint64_t flushed = 0;
using Clock = std::chrono::steady_clock;
static uint64_t Micros(Clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t)
      .count();
}
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string profile = argv[1];
  const bool archive = profile.find("archive") != std::string::npos;
  const bool native = profile.find("native") != std::string::npos;
  const bool freeze = profile.find("frozen") != std::string::npos;
  lv_init();
  lv_image_cache_resize(2 * 1024 * 1024, true);
  auto *display = lv_display_create(480, 480);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, draw_rows, nullptr, sizeof(draw_rows),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(
      display, [](lv_display_t *d, const lv_area_t *a, uint8_t *) {
        flushed += uint64_t(a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
        lv_display_flush_ready(d);
      });
  StoryFont font(native
                     ? "vendor/xiaozhi-esp32/managed_components/"
                       "78__xiaozhi-fonts/cbin/font_noto_sans_common_16_4.bin"
                     : "vendor/xiaozhi-esp32/managed_components/"
                       "78__xiaozhi-fonts/cbin/font_noto_sans_common_30_4.bin");
  lv_obj_remove_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);
  spring::Core core;
  core.storage_ok = true;
  core.EnableEconomy(true);
  core.EnableMemory(1, 2);
  core.EnableStory();
  core.Wake();
  core.s.tree_stage = 2;
  core.s.economy.built = 0xef;
  core.s.economy.animals = 3;
  auto state = core.Get();
  spring_art::Create(lv_screen_active());
  spring_art::AdvanceMotion(state, 1000, true, spring_gardener::Voice::Speak);
  spring_art::Update(state, 1000, true, false, true);
  lv_refr_now(display);
  const lv_image_dsc_t *window = nullptr;
  lv_obj_t *robot = nullptr;
  for (unsigned i = 0; i < lv_obj_get_child_count(spring_art::World()); ++i) {
    auto *child = lv_obj_get_child(spring_art::World(), i);
    if (!lv_obj_check_type(child, &lv_image_class))
      continue;
    auto *image = static_cast<const lv_image_dsc_t *>(lv_image_get_src(child));
    if (image && image->header.w == 480 &&
        image->header.h == spring_scenery::kHeight)
      window = image;
    if (image == spring_art::Get(spring_art::Id::Hover_hover_0).image ||
        image == spring_art::Get(spring_art::Id::Hover_speak_0).image)
      robot = child;
  }
  assert(window && robot);
  const std::vector<uint8_t> initial_window(window->data,
                                            window->data + window->data_size);
  std::set<const void *> robot_frames;
  lv_mem_monitor_t before{}, after{};
  lv_mem_monitor(&before);
  auto &allocator = LV_GLOBAL_DEFAULT()->tlsf_state;
  const auto baseline_used = allocator.cur_used;
  allocator.max_used = baseline_used;
  auto *card = spring_ui::Target(
      lv_layer_top(),
      {archive ? 8 : 34, 76, archive ? 464 : 412, archive ? 364 : 190});
  lv_obj_set_style_bg_color(card, lv_color_hex(0xF2ECD8), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  auto label = [&](lv_obj_t *parent, int x, int y, int width, int target,
                   const char *text) {
    auto *l = lv_label_create(parent);
    spring_ui::Passive(l);
    lv_obj_set_style_text_font(l, &font.font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x293A28), 0);
    const int scale = native ? 256 : target * 256 / font.font.line_height;
    lv_obj_set_style_transform_scale_x(l, scale, 0);
    lv_obj_set_style_transform_scale_y(l, scale, 0);
    lv_obj_set_style_transform_pivot_x(l, 0, 0);
    lv_obj_set_style_transform_pivot_y(l, 0, 0);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, width * 256 / scale);
    lv_label_set_text(l, text);
    return l;
  };
  label(card, 20, 14, archive ? 350 : 370, 26,
        archive ? "无人接收的养护报告" : "与01-99通讯");
  if (archive) {
    auto *scroll = spring_ui::Target(card, {20, 52, 424, 128});
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    auto *content = spring_ui::Target(scroll, {0, 0, 422, 1});
    auto *text = label(content, 2, 2, 398, 22, spring::kStoryArchiveTexts[0]);
    lv_obj_update_layout(text);
    const auto scale = lv_obj_get_style_transform_scale_y(text, LV_PART_MAIN);
    lv_obj_set_height(content, lv_obj_get_height(text) * scale / 256 + 6);
  } else
    label(card, 20, 54, 370, 24,
          "01-99想和你说说\n刚才完成的检查\n可以继续追问。");
  auto *status =
      label(card, 20, archive ? 188 : 144, 220, 20, "01-99 正在说话");
  lv_obj_update_layout(card);
  const auto first_started = Clock::now();
  lv_refr_now(display);
  const auto first_draw_us = Micros(first_started);
  uint64_t update_us = 0, draw_us = 0, max_update_us = 0, max_draw_us = 0;
  flushed = 0;
  for (unsigned i = 1; i <= 160; ++i) {
    const uint64_t now = 1000 + i * 50;
    lv_tick_inc(50);
    spring_art::AdvanceMotion(state, now, true, spring_gardener::Voice::Speak);
    auto at = Clock::now();
    // Freezing is a host control, not an assertion about a deployed fix.
    if (!freeze)
      spring_art::Update(state, now, true, false, true);
    robot_frames.insert(lv_image_get_src(robot));
    const auto update = Micros(at);
    update_us += update;
    max_update_us = std::max(max_update_us, update);
    if (i % 20 == 0)
      spring_ui::TextChanged(status, i % 40 ? "正在聆听" : "01-99 正在说话");
    at = Clock::now();
    lv_refr_now(display);
    const auto draw = Micros(at);
    draw_us += draw;
    max_draw_us = std::max(max_draw_us, draw);
  }
  lv_mem_monitor(&after);
  assert(
      std::equal(initial_window.begin(), initial_window.end(), window->data));
  if (!freeze)
    assert(robot_frames.size() > 1);
  // Reading without voice uses the explicit decoration budget too.
  for (uint64_t now = 9200; now <= 10000; now += 100) {
    spring_art::AdvanceMotion(state, now, false);
    spring_art::Update(state, now, false, false, false, false);
  }
  assert(
      std::equal(initial_window.begin(), initial_window.end(), window->data));
  // Voice ends: scenery catches up using real elapsed time, with no new clock.
  spring_art::Update(state, 12000, false, false, false);
  assert(
      !std::equal(initial_window.begin(), initial_window.end(), window->data));
  std::cout << "{\"host_only\":true,\"profile\":\"" << profile
            << "\",\"font_line_height\":" << font.font.line_height
            << ",\"additional_peak_bytes\":" << after.max_used - baseline_used
            << ",\"retained_delta_bytes\":"
            << int(before.free_size) - int(after.free_size)
            << ",\"first_draw_us\":" << first_draw_us
            << ",\"scene_update_total_us\":" << update_us
            << ",\"scene_update_max_us\":" << max_update_us
            << ",\"draw_total_us\":" << draw_us
            << ",\"draw_max_us\":" << max_draw_us
            << ",\"flushed_pixels\":" << flushed
            << ",\"robot_animation_frames\":" << robot_frames.size()
            << ",\"frames\":160,\"device_fps_measured\":false}\n";
}
