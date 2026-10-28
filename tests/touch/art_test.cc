#include "spring_art.h"
#include "spring_scenery.h"
#include "spring_touch.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <src/draw/lv_image_decoder_private.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <string>
#include <vector>
static uint8_t frame[480 * 480 * 4];
static void Capture(const std::string &name) {
  std::ofstream file(name, std::ios::binary);
  file << "P6\n480 480\n255\n";
  for (unsigned i = 0; i < 480 * 480; ++i) {
    const char rgb[] = {static_cast<char>(frame[i * 4 + 2]),
                        static_cast<char>(frame[i * 4 + 1]),
                        static_cast<char>(frame[i * 4])};
    file.write(rgb, 3);
  }
}
static void UpdateFrame(const spring::Snapshot &s, uint64_t now, bool talking) {
  spring_art::AdvanceMotion(s, now, talking);
  spring_art::Update(s, now, talking);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  lv_init();
  auto *display = lv_display_create(480, 480);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
  lv_display_set_buffers(display, frame, nullptr, sizeof(frame),
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display,
                          [](lv_display_t *d, const lv_area_t *, uint8_t *) {
                            lv_display_flush_ready(d);
                          });
  // Decode every device image using the exact vendored LVGL decoder, not
  // Pillow.
  for (unsigned i = 0; i < static_cast<unsigned>(spring_art::Id::Count); ++i) {
    const auto &asset = spring_art::Get(static_cast<spring_art::Id>(i));
    assert(asset.image->header.w && asset.image->header.h);
    lv_image_decoder_dsc_t decoded{};
    assert(lv_image_decoder_open(&decoded, asset.image, nullptr) ==
           LV_RESULT_OK);
    if (decoded.decoded == nullptr) {
      lv_area_t full{0, 0, static_cast<int32_t>(asset.image->header.w) - 1,
                     static_cast<int32_t>(asset.image->header.h) - 1};
      lv_area_t area{LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN};
      assert(lv_image_decoder_get_area(&decoded, &full, &area) == LV_RESULT_OK);
    }
    lv_image_decoder_close(&decoded);
    lv_image_cache_drop(asset.image);
  }
  auto *stage = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(stage);
  lv_obj_set_size(stage, 480, 480);
  spring_ui::Passive(stage);
  spring_art::Create(stage);
  lv_obj_t *robot_object = nullptr;
  for (unsigned i = 0; i < lv_obj_get_child_count(spring_art::World()); ++i) {
    auto *child = lv_obj_get_child(spring_art::World(), i);
    if (lv_obj_check_type(child, &lv_image_class) &&
        lv_image_get_src(child) ==
            spring_art::Get(spring_art::Id::Hover_hover_0).image)
      robot_object = child;
  }
  assert(robot_object);
  auto assert_robot_group = [&](spring_art::Id base) {
    const auto *src = lv_image_get_src(robot_object);
    bool match = false;
    for (unsigned i = 0; i < 4; ++i)
      match |= src == spring_art::Get(static_cast<spring_art::Id>(
                                          static_cast<unsigned>(base) + i))
                          .image;
    assert(match);
  };

  int minimum_x = 480, maximum_x = 0;
  spring::Core game;
  game.storage_ok = true;
  game.Wake();
  auto initial = game.Get();
  UpdateFrame(initial, 1000, false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/art-device-initial.ppm");
  auto grown = initial;
  grown.tree_stage = 1;
  for (auto &p : grown.plots)
    p.ready = true;
  grown.work = spring::Work::Rest;
  UpdateFrame(grown, 5000, false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/art-device-grown.ppm");
  // Uneven display intervals must not teleport the independently stepped flyer.
  auto walking = grown;
  walking.work = spring::Work::Build;
  walking.economy.construction = 0;
  UpdateFrame(walking, 6000, false);
  float previous_anchor = 0;
  for (unsigned i = 0; i < 12; ++i) {
    UpdateFrame(walking, 6400 + i * 470, false);
    lv_obj_update_layout(stage);
    auto src = lv_image_get_src(robot_object);
    unsigned id = 0;
    for (; id < static_cast<unsigned>(spring_art::Id::Count); ++id)
      if (src == spring_art::Get(static_cast<spring_art::Id>(id)).image)
        break;
    assert(id >= static_cast<unsigned>(spring_art::Id::Hover_hover_0));
    float anchor = lv_obj_get_x(robot_object) -
                   spring_art::Get(static_cast<spring_art::Id>(id)).x;
    if (i)
      assert(std::abs(anchor - previous_anchor) <= 3);
    previous_anchor = anchor;
  }
  auto canopy = grown;
  canopy.tree_stage = 2;
  UpdateFrame(canopy, 12500, false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/art-device-canopy.ppm");
  size_t mid_free = 0;
  for (unsigned i = 0; i < 480; ++i) {
    lv_tick_inc(100);
    grown.work = static_cast<spring::Work>((i / 35) % 4);
    grown.target = (i / 20) % 3;
    grown.dew_path = (i % 20) / 20.f;
    UpdateFrame(grown, 14000 + i * 100, i % 50 > 40);
    lv_refr_now(display);
    lv_area_t robot_bounds;
    lv_obj_get_coords(robot_object, &robot_bounds);
    assert(robot_bounds.x1 >= 0 && robot_bounds.x2 < 480);
    assert(robot_bounds.y1 >= 0 && robot_bounds.y2 < 480);
    minimum_x = std::min(minimum_x, robot_bounds.x1);
    maximum_x = std::max(maximum_x, robot_bounds.x1);
    assert(lv_obj_get_scroll_x(spring_art::Viewport()) == 0);
    if (i == 239) {
      lv_mem_monitor_t memory;
      lv_mem_monitor(&memory);
      mid_free = memory.free_size;
    }
  }
  lv_mem_monitor_t memory;
  lv_mem_monitor(&memory);
  assert(memory.free_size + 32 * 1024 >=
         mid_free); // No accumulating frame allocations.
  assert(maximum_x - minimum_x >
         30); // Walking is preserved, not a stationary sprite.
  // Quiet/sleep never own spatial simulation: keep stepping the worker while
  // deliberately omitting renderer updates, then show the actual current pose.
  auto offscreen = grown;
  offscreen.work = spring::Work::Water;
  offscreen.target = 0;
  for (unsigned t = 70000; t < 110000; t += 100)
    spring_art::AdvanceMotion(offscreen, t, false);
  spring_art::Update(offscreen, 110000, false, true);
  lv_obj_update_layout(stage);
  auto anchor_x = [&] {
    const auto src = lv_image_get_src(robot_object);
    for (unsigned i = 0; i < static_cast<unsigned>(spring_art::Id::Count);
         ++i) {
      const auto &a = spring_art::Get(static_cast<spring_art::Id>(i));
      if (a.image == src)
        return lv_obj_get_x(robot_object) - a.x;
    }
    assert(false);
    return 0;
  };
  assert(std::abs(anchor_x() - 204) <= 5);
  offscreen.target = 2;
  for (unsigned t = 110000; t < 150000; t += 100)
    spring_art::AdvanceMotion(offscreen, t, false);
  spring_art::Update(offscreen, 150000, false);
  lv_obj_update_layout(stage);
  assert(std::abs(anchor_x() - 294) <= 5);
  lv_obj_t *targets[3];
  for (unsigned i = 0; i < 3; ++i) {
    const auto rect = spring_ui::kWorldPlots[i];
    targets[i] = spring_ui::Target(spring_art::World(), rect);
    lv_obj_update_layout(stage);
    lv_point_t point{rect.x + rect.width / 2, rect.y + rect.height / 2};
    assert(lv_indev_search_obj(stage, &point) == targets[i]);
    assert(rect.x >= 0 && rect.x + rect.width <= 480);
  }
  const auto tree_rect = spring_ui::kWorldTree;
  auto *tree_target = spring_ui::Target(spring_art::World(), tree_rect);
  lv_obj_update_layout(stage);
  lv_point_t tree_point{tree_rect.x + tree_rect.width / 2,
                        tree_rect.y + tree_rect.height / 2};
  assert(lv_indev_search_obj(stage, &tree_point) == tree_target);
  // The actual new hit rectangles: icon children never intercept their parent,
  // and the radio remains independent of the larger plants and tree.
  auto *hud = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(hud);
  spring_ui::Passive(hud);
  lv_obj_set_size(hud, 480, 480);
  for (unsigned i = 0; i < 5; ++i) {
    auto r = spring_ui::kResources[i];
    auto *target = spring_ui::Target(hud, r);
    lv_obj_set_style_bg_color(target, lv_color_hex(0x2F392C), 0);
    lv_obj_set_style_bg_opa(target, LV_OPA_90, 0);
    lv_obj_set_style_radius(target, 22, 0);
    const spring_art::Id icons[] = {
        spring_art::Id::Icon_fiber, spring_art::Id::Icon_water,
        spring_art::Id::Icon_energy, spring_art::Id::Icon_food,
        spring_art::Id::Icon_parts};
    auto *icon = spring_art::Image(target, icons[i], 3, 9);
    (void)icon;
    auto *amount = lv_label_create(target);
    spring_ui::Passive(amount);
    lv_label_set_text(amount, i == 0 ? "59" : i == 1 ? "20" : "99");
    lv_obj_set_style_text_color(amount, lv_color_hex(0xF5EFDA), 0);
    lv_obj_set_style_transform_scale(amount, 256, 0);
    lv_obj_set_style_transform_pivot_x(amount, 0, 0);
    lv_obj_set_style_transform_pivot_y(amount, 0, 0);
    lv_obj_set_pos(amount, 29, 10);
    lv_obj_update_layout(hud);
    lv_point_t point{r.x + 20, r.y + 20};
    assert(lv_indev_search_obj(lv_layer_top(), &point) == target);
  }
  auto *radio = spring_ui::Target(hud, spring_ui::kCommunication);
  lv_obj_set_style_bg_color(radio, lv_color_hex(0x354A35), 0);
  lv_obj_set_style_bg_opa(radio, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(radio, 36, 0);
  lv_obj_set_style_border_width(radio, 2, 0);
  lv_obj_set_style_border_color(radio, lv_color_hex(0xC4C6A1), 0);
  spring_ui::RadioGlyph(radio);
  auto *log = spring_ui::Target(hud, spring_ui::kJournal);
  lv_obj_set_style_bg_color(log, lv_color_hex(0x2F392C), 0);
  lv_obj_set_style_bg_opa(log, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(log, 16, 0);
  spring_art::Image(log, spring_art::Id::Icon_journal, 8, 8);
  lv_obj_update_layout(hud);
  lv_point_t radio_point{50, 402}, log_point{440, 36};
  assert(lv_indev_search_obj(lv_layer_top(), &radio_point) == radio);
  assert(lv_indev_search_obj(lv_layer_top(), &log_point) == log);
  assert(spring_ui::NextCommunication(0) == 99);
  assert(spring_ui::NextCommunication(15) == 2);
  assert(spring_ui::NextCommunication(3) == 0);
  for (unsigned i = 0; i < 3; ++i) {
    auto r = spring_ui::kWorldPlots[i];
    lv_point_t point{r.x + r.width / 2, r.y + r.height / 2};
    assert(lv_indev_search_obj(lv_layer_top(), &point) == nullptr);
    assert(lv_indev_search_obj(stage, &point) == targets[i]);
  }
  spring_art::Update(offscreen, 150500, false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/scene-v2-native-main.ppm");
  auto conversation = offscreen;
  conversation.work = spring::Work::Rest;
  conversation.work_ms = 0;
  conversation.economy.construction = 255;
  conversation.economy.recycle_ms = conversation.economy.journey_work = 0;
  conversation.tree_work_ms = conversation.economy.emergency_work = 0;
  spring_art::AdvanceMotion(conversation, 151000, true,
                            spring_gardener::Voice::Listen);
  assert(std::string(spring_art::GardenerMode()) == "listen");
  spring_art::Update(conversation, 151000, true, false, false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/scene-v2-listening.ppm");
  spring_art::AdvanceMotion(conversation, 151250, true,
                            spring_gardener::Voice::Wait);
  assert(std::string(spring_art::GardenerMode()) == "listen");
  spring_art::AdvanceMotion(conversation, 151500, true,
                            spring_gardener::Voice::Speak);
  assert(std::string(spring_art::GardenerMode()) == "speak");
  spring_art::Update(conversation, 151500, true, false, true);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/scene-v2-speaking.ppm");
  auto economy = offscreen;
  economy.economy.enabled = true;
  economy.economy.built = 255;
  economy.economy.animals = 3;
  economy.economy.fourth.ready = true;
  economy.economy.fourth.growth_ms = 600000;
  economy.economy.food = 20;
  economy.tree_stage = 2;
  spring_art::Update(economy, 152000, false);
  lv_refr_now(display);
  Capture(std::string(argv[1]) + "/economy-all-facilities.ppm");
  // New life art runs through the actual LVGL renderer, including four-slot
  // scenery.
  auto life_scene = economy;
  life_scene.work = spring::Work::Rest;
  life_scene.work_ms = 0;
  life_scene.economy.construction = 255;
  life_scene.economy.recycle_ms = life_scene.economy.journey_work = 0;
  life_scene.economy.emergency_work = life_scene.tree_work_ms = 0;
  life_scene.life.intent = 3;
  life_scene.life.phase = 3;
  life_scene.life.reason = 3;
  life_scene.life.under_tree = life_scene.life.reached = 1;
  for (uint64_t t = 152100; t < 220000; t += 100)
    UpdateFrame(life_scene, t, false);
  assert(spring_art::GardenerLifeArrived(life_scene));
  lv_refr_now(display);
  assert_robot_group(spring_art::Id::Life_rest_tree_0);
  Capture(std::string(argv[1]) + "/life-rest-tree-new.ppm");
  life_scene.life.intent = 2;
  life_scene.life.reason = 2;
  life_scene.life.target = 0;
  life_scene.life.under_tree = 0;
  for (uint64_t t = 220000; t < 280000; t += 100)
    UpdateFrame(life_scene, t, false);
  lv_refr_now(display);
  assert_robot_group(spring_art::Id::Life_inspect_left_0);
  Capture(std::string(argv[1]) + "/life-inspect-left-new.ppm");
  life_scene.life.target = 3;
  for (uint64_t t = 280000; t < 340000; t += 100)
    UpdateFrame(life_scene, t, false);
  lv_refr_now(display);
  assert_robot_group(spring_art::Id::Life_care_0);
  Capture(std::string(argv[1]) + "/life-care-new.ppm");
  life_scene.life = {};
  life_scene.work = spring::Work::Harvest;
  life_scene.work_ms = 2500;
  life_scene.target = 1;
  for (uint64_t t = 340000; t <= 341000; t += 100)
    UpdateFrame(life_scene, t, false);
  lv_refr_now(display);
  assert_robot_group(spring_art::Id::Life_harvest_0);
  Capture(std::string(argv[1]) + "/life-harvest-new.ppm");
  const lv_image_dsc_t *window_pixels = nullptr;
  for (unsigned i = 0; i < lv_obj_get_child_count(spring_art::World()); ++i) {
    auto *child = lv_obj_get_child(spring_art::World(), i);
    if (!lv_obj_check_type(child, &lv_image_class))
      continue;
    const auto *image =
        static_cast<const lv_image_dsc_t *>(lv_image_get_src(child));
    if (image && image->header.w == 480 &&
        image->header.h == spring_scenery::kHeight)
      window_pixels = image;
  }
  assert(window_pixels);
  const auto *original_bg = reinterpret_cast<const uint16_t *>(
      spring_art::Get(spring_art::Id::Background).image->data);
  for (unsigned region = 0; region < 5; ++region) {
    auto scene = offscreen;
    scene.economy.region = region;
    scene.tree_stage = 0;
    spring_art::Update(scene, 342000 + region * 1000, false);
    lv_refr_now(display);
    Capture(std::string(argv[1]) + "/window-region-" + std::to_string(region) +
            ".ppm");
    if (region) {
      for (int y = 129; y < 235; ++y)
        for (int x = 0; x < 140; ++x)
          if (spring_scenery::Rack(x, y)) {
            const auto *row = reinterpret_cast<const uint16_t *>(
                window_pixels->data +
                (y - spring_scenery::kTop) * window_pixels->header.stride);
            assert(row[x] == original_bg[y * 480 + x]);
          }
      std::vector<uint8_t> before(
          window_pixels->data, window_pixels->data + window_pixels->data_size);
      spring_art::Update(scene, 342000 + region * 1000 + 1000, false);
      unsigned changed = 0;
      for (unsigned i = 0; i < before.size(); ++i)
        if (before[i] != window_pixels->data[i])
          ++changed;
      assert(changed > 10000);
      lv_refr_now(display);
      Capture(std::string(argv[1]) + "/window-region-" +
              std::to_string(region) + "-moving.ppm");
    }
  }
  std::cout << static_cast<unsigned>(spring_art::Id::Count)
            << " native images decoded; 480 updates, memory plateau; all "
               "plots/tree visible; hovering bounded; fixed camera.\n";
  lv_deinit();
}
