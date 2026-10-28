#include "spring_art.h"
#include <src/misc/cache/instance/lv_image_cache.h>
#include <algorithm>
#include <cmath>
#include <mutex>
#include "spring_scenery.h"
#include "spring_story_art.h"
#include "spring_touch.h"
#ifdef ESP_PLATFORM
#include "spring_external_art.h"
#endif
namespace spring_art {
namespace {
lv_obj_t *window_band, *plants[4], *tree, *robot, *dew, *decor[3], *channels[5], *rewards[3],
    *drops[3];
lv_obj_t *voice_marks[3], *voice_speaker, *voice_receiver;
lv_obj_t *planters[4], *facilities[8], *animals[2], *tree_home, *shadow;
Id robot_id = Id::Count, tree_id = Id::Count,
   plant_ids[4] = {Id::Count, Id::Count, Id::Count, Id::Count};
lv_obj_t *world_root, *viewport;
float robot_x = 220;
std::mutex motion_mutex;
spring_gardener::Controller gardener;
uint64_t scenery_at = 0, robot_at = 0, reward_at[3]{};
uint32_t previous_resources[3]{};
bool previous_demo = false, have_previous = false, created = false;
lv_draw_buf_t* band_buffer = nullptr;
lv_image_dsc_t band_image{};
lv_obj_t *story_flower = nullptr, *story_leaf = nullptr, *story_hint = nullptr;
unsigned story_flower_phase = 255;
unsigned scenery_region = 0;
struct Rgba {
    unsigned r, g, b, a;
};
Rgba Indexed(const lv_image_dsc_t* image, int x, int y) {
    const auto* data = image->data;
    const auto packed = data[64 + y * image->header.stride + x / 2];
    const auto index = x % 2 ? packed & 15 : packed >> 4;
    const auto* color = data + index * 4;
    return {color[2], color[1], color[0], color[3]};
}
Rgba Blend(Rgba foreground, Rgba background) {
    if (foreground.a == 255)
        return foreground;
    if (!foreground.a)
        return background;
    const auto a = foreground.a, inverse = 255 - a;
    return {(foreground.r * a + background.r * inverse + 127) / 255,
            (foreground.g * a + background.g * inverse + 127) / 255,
            (foreground.b * a + background.b * inverse + 127) / 255, 255};
}
uint16_t Pack(Rgba c) { return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3); }
// Reuse a small RGB565 foliage band; everything behind transparent leaves is
// copied from the SAME static background, so no horizon/sky strip can split it.
void ComposeWindow(uint64_t now) {
    if (!band_buffer)
        return;
    if (Get(Id::Background).image->header.w != 480)
        return;
    const auto* background = reinterpret_cast<const uint16_t*>(Get(Id::Background).image->data);
    const auto* near = Get(Id::Near).image;
    const int offset = (now / 250) % 960;
    const unsigned near_offset = ((now * 6) / 1000) % (spring_scenery::kNearWidth * 2);
    for (int row = 0; row < spring_scenery::kHeight; ++row) {
        const int y = row + spring_scenery::kTop;
        auto* output =
            reinterpret_cast<uint16_t*>(band_buffer->data + row * band_buffer->header.stride);
        for (int x = 0; x < 480; ++x) {
            const auto pixel = background[y * 480 + x];
            output[x] = pixel;
            if (scenery_region > 0 && scenery_region < 5) {
                if (spring_scenery::Glass(x, y)) {
                    const auto f = spring_scenery::Far(scenery_region, x, y);
                    const auto n = spring_scenery::Near(scenery_region, x, y, near_offset);
                    output[x] = Pack(Blend({n.r, n.g, n.b, n.a}, {f.r, f.g, f.b, f.a}));
                }
                continue;
            }
            // Restrict moving foliage to clear glass, away from the baked empty rack.
            const bool glass = x >= 153 && x < 328 && y >= 158 && y < 240;
            if (!glass)
                continue;
            const int u = (x + offset) % 960;
            const auto leaf = Indexed(near, u < 480 ? u : 959 - u, y - 158);
            if (!leaf.a)
                continue;
            Rgba color{static_cast<unsigned>((pixel >> 11) << 3),
                       static_cast<unsigned>(((pixel >> 5) & 63) << 2),
                       static_cast<unsigned>((pixel & 31) << 3), 255};
            output[x] = Pack(Blend(leaf, color));
        }
    }
    if (window_band)
        lv_obj_invalidate(window_band);
}
void Visible(lv_obj_t* object, bool visible) {
    const bool hidden = lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN);
    if (visible && hidden)
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else if (!visible && !hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}
void Place(lv_obj_t* object, Id id, int x, int y) {
    const auto& asset = Get(id);
    lv_obj_set_pos(object, x + asset.x, y + asset.y);
}
lv_obj_t* Block(lv_obj_t* parent, int x, int y, int w, int h) {
    auto* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    spring_ui::Passive(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    return object;
}
}  // namespace
lv_obj_t* Image(lv_obj_t* parent, Id id, int x, int y) {
    auto* image = lv_image_create(parent);
    spring_ui::Passive(image);
    lv_image_set_src(image, Get(id).image);
    Place(image, id, x, y);
    return image;
}
lv_obj_t* World() { return world_root; }
lv_obj_t* FacilityObject(unsigned index) {
    return index < 8 ? facilities[index] : index == 8 ? tree_home : nullptr;
}
lv_obj_t* Viewport() { return viewport; }
void Create(lv_obj_t* parent) {
    viewport = Block(parent, 0, 0, 480, 480);
    world_root = viewport;
    parent = world_root;
    Image(parent, Id::Background, 0, 0);
    band_buffer = Get(Id::Background).image->header.w == 480
                      ? lv_draw_buf_create(480, spring_scenery::kHeight, LV_COLOR_FORMAT_RGB565, 0)
                      : nullptr;
    if (band_buffer) {
        band_image.header.magic = LV_IMAGE_HEADER_MAGIC;
        band_image.header.cf = LV_COLOR_FORMAT_RGB565;
        band_image.header.w = 480;
        band_image.header.h = spring_scenery::kHeight;
        band_image.header.stride = band_buffer->header.stride;
        band_image.data_size = band_buffer->data_size;
        band_image.data = band_buffer->data;
        ComposeWindow(0);
        window_band = lv_image_create(parent);
        spring_ui::Passive(window_band);
        lv_image_set_src(window_band, &band_image);
        lv_obj_set_pos(window_band, 0, spring_scenery::kTop);
        lv_obj_add_event_cb(
            parent,
            [](lv_event_t*) {
                lv_image_cache_drop(&band_image);
                lv_draw_buf_destroy(band_buffer);
                band_buffer = nullptr;
                window_band = nullptr;
                created = false;
            },
            LV_EVENT_DELETE, nullptr);
    }
    story_flower = lv_image_create(parent);
    spring_ui::Passive(story_flower);
    lv_image_set_src(story_flower, StoryFlowerImage(0));
    lv_obj_set_pos(story_flower, 166, 160);
    Visible(story_flower, false);
    channels[0] = Block(parent, 100, 408, 290, 2);
    for (int i = 0; i < 4; ++i)
        channels[i + 1] = Block(parent, 70 + 81 * i, 356, 2, 54);
    for (auto* channel : channels) {
        lv_obj_set_style_bg_color(channel, lv_color_hex(0x96B98C), 0);
        lv_obj_set_style_bg_opa(channel, LV_OPA_60, 0);
    }
    tree = Image(parent, Id::TreeYoung, spring_ui::kTreeX, spring_ui::kTreeGroundY);
    for (unsigned i = 0; i < 4; ++i) {
        planters[i] = Image(parent, Id::Planter, 130 + 104 * i - 51, 389);
        plants[i] = Image(parent, Id::Plant_tracklight_seedling, 130 + 104 * i, spring_ui::kSoilY);
        Visible(planters[i], i < 3);
        Visible(plants[i], i < 3);
    }
    const int fx[] = {313, 107, 13, 24, 395, 13, 432, 370},
              fy[] = {277, 219, 82, 184, 383, 267, 365, 127};
    for (unsigned i = 0; i < 8; ++i) {
        facilities[i] = Image(parent, static_cast<Id>(static_cast<unsigned>(Id::Facility_tank) + i),
                              fx[i], fy[i]);
        Visible(facilities[i], false);
    }
    tree_home = Image(parent, Id::TreeHome, 394, 206);
    Visible(tree_home, false);
    animals[0] = Image(parent, Id::Animal_bird, 411, 142);
    animals[1] = Image(parent, Id::Animal_mouse, 428, 252);
    for (auto* object : animals)
        Visible(object, false);
    decor[0] = Image(parent, Id::Prop_ivy, 38, 18);
    decor[1] = Image(parent, Id::Prop_ivy, 412, 18);
    decor[2] = Image(parent, Id::Prop_moss, 384, 390);
    shadow = Block(parent, 226, 361, 40, 8);
    lv_obj_set_style_radius(shadow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(shadow, lv_color_hex(0x23362F), 0);
    lv_obj_set_style_bg_opa(shadow, LV_OPA_20, 0);
    // Original passenger furniture, clear of foreground plants; no click/build action.
    Image(parent, Id::Prop_passenger_seat, 240, 338);
    robot = Image(parent, Id::Hover_hover_0, 246, 292);
    story_leaf = Image(parent, Id::Icon_leaf, 0, 0);
    lv_image_set_scale(story_leaf, 110);
    Visible(story_leaf, false);
    story_hint = Block(parent, 0, 0, 12, 12);
    lv_obj_set_style_radius(story_hint, 6, 0);
    lv_obj_set_style_bg_color(story_hint, lv_color_hex(0xE9E4A4), 0);
    Visible(story_hint, false);
    dew = Image(parent, Id::Icon_dew, 224, 320);
    for (unsigned i = 0; i < 3; ++i) {
        rewards[i] =
            Image(parent, static_cast<Id>(static_cast<unsigned>(Id::Icon_water) + i), 0, 0);
        Visible(rewards[i], false);
    }
    for (auto* object : decor)
        Visible(object, false);
    for (auto& drop : drops) {
        drop = Block(parent, 0, 0, 3, 5);
        lv_obj_set_style_bg_color(drop, lv_color_hex(0xBCEAF0), 0);
        lv_obj_set_style_bg_opa(drop, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(drop, 3, 0);
        Visible(drop, false);
    }
    for (unsigned i = 0; i < 3; ++i) {
        auto* arc = lv_arc_create(parent);
        voice_marks[i] = arc;
        lv_obj_remove_style_all(arc);
        spring_ui::Passive(arc);
        const int radius = 9 + 4 * i;
        lv_obj_set_size(arc, radius * 2, radius * 2);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_obj_set_style_arc_width(arc, 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0xB9EEF2), LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
        Visible(arc, false);
    }
    voice_speaker = Block(parent, 0, 0, 16, 20);
    auto* speaker_base = Block(voice_speaker, 0, 7, 5, 7);
    lv_obj_set_style_bg_color(speaker_base, lv_color_hex(0xB9EEF2), 0);
    lv_obj_set_style_bg_opa(speaker_base, LV_OPA_COVER, 0);
    static const lv_point_precise_t cone[] = {{4, 7}, {12, 2}, {12, 18}, {4, 13}};
    auto* speaker_cone = lv_line_create(voice_speaker);
    spring_ui::Passive(speaker_cone);
    lv_line_set_points(speaker_cone, cone, 4);
    lv_obj_set_style_line_width(speaker_cone, 2, 0);
    lv_obj_set_style_line_color(speaker_cone, lv_color_hex(0xB9EEF2), 0);
    voice_receiver = Block(parent, 0, 0, 4, 4);
    lv_obj_set_style_radius(voice_receiver, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(voice_receiver, lv_color_hex(0xB9EEF2), 0);
    lv_obj_set_style_bg_opa(voice_receiver, LV_OPA_COVER, 0);
    Visible(voice_speaker, false);
    Visible(voice_receiver, false);
    lv_obj_update_layout(viewport);
    created = true;
#ifdef ESP_PLATFORM
    if (!GetBackgroundMemory().ready) {
        // Keep the error visible above game HUD/modals and prevent accidental spending.
        auto* error = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(error);
        lv_obj_set_size(error, 480, 480);
        lv_obj_set_style_bg_color(error, lv_color_hex(0x202C28), 0);
        lv_obj_set_style_bg_opa(error, LV_OPA_COVER, 0);
        lv_obj_add_flag(error, LV_OBJ_FLAG_CLICKABLE);
        auto* label = lv_label_create(error);
        lv_obj_set_width(label, 320);
        lv_label_set_text(label, "素材加载失败\n请恢复配套素材包\nAsset error");
        lv_obj_set_style_text_color(label, lv_color_hex(0xF4EDDB), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    }
#endif
}
// Called by the unchanged 100ms game worker in ALL display modes. Position
// advances independently of frame rendering, including while the screen is off.
void AdvanceMotion(const spring::Snapshot& s, uint64_t now, bool talking,
                   spring_gardener::Voice voice) {
    std::lock_guard<std::mutex> guard(motion_mutex);
    if (talking && voice == spring_gardener::Voice::None)
        voice = spring_gardener::Voice::Listen;
    gardener.Step(s, now, voice);
}
void GardenerActivity(uint64_t now) {
    std::lock_guard<std::mutex> guard(motion_mutex);
    gardener.Activity(now);
}
bool GardenerWave(uint64_t now) {
    std::lock_guard<std::mutex> guard(motion_mutex);
    return gardener.Wave(now);
}
bool ReactShake(uint64_t now, uint32_t random, spring_gardener::Voice voice, bool harvested) {
    std::lock_guard<std::mutex> guard(motion_mutex);
    return gardener.Shake(now, random, voice, harvested);
}
bool GardenerLifeArrived(const spring::Snapshot& s) {
    std::lock_guard<std::mutex> guard(motion_mutex);
    return spring_gardener::LifeArrived(s, gardener.Get());
}
spring_gardener::Pose GardenerPose() {
    std::lock_guard<std::mutex> guard(motion_mutex);
    return gardener.Get();
}
const char* GardenerMode() {
    std::lock_guard<std::mutex> guard(motion_mutex);
    return spring_gardener::Name(gardener.Get().mode);
}
void Update(const spring::Snapshot& s, uint64_t now, bool talking, bool quiet, bool speaking,
            bool animate_scenery) {
    (void)speaking;
    if (!created)
        return;
    if (s.story.enabled && story_flower) {
        Visible(story_flower, s.story.flower > 0);
        if (s.story.flower && story_flower_phase != s.story.flower) {
            story_flower_phase = s.story.flower;
            lv_image_set_src(story_flower, StoryFlowerImage(s.story.flower - 1));
        }
        const auto pose = GardenerPose();
        Visible(story_leaf, s.story.new_leaf);
        lv_obj_set_pos(story_leaf, static_cast<int>(pose.x) + 6, static_cast<int>(pose.y) - 60);
        const bool hint = s.life.phase == 3 && s.life.reached && !(s.story.life_attention & 1) &&
                          s.life.intent == 3 && s.story.elapsed - s.story.life_prompt_at < 4000;
        Visible(story_hint, hint);
        lv_obj_set_pos(story_hint, static_cast<int>(pose.x) + 27, static_cast<int>(pose.y) - 55);
    }
    // Only nearby foliage changes; the far view and floor remain static.
    // The 480x160 band overlaps the call/archive card. Invalidating it redraws
    // even unchanged transformed text in that card. Leave the decorative band
    // still during voice/modals; all real robot movement below continues.
    if (animate_scenery && !talking &&
        (scenery_region != s.economy.region || now - scenery_at >= (quiet ? 2000u : 500u))) {
        scenery_region = s.economy.region;
        ComposeWindow(now);
        scenery_at = now;
    }
    for (unsigned i = 0; i < 4; ++i) {
        const bool shown = i < PlotCount(s);
        Visible(plants[i], shown);
        Visible(planters[i], shown);
        if (!shown)
            continue;
        const auto& p = i < 3 ? s.plots[i] : s.economy.fourth;
        const auto duration =
            s.economy.enabled ? 600000u : spring::kGrowMs[static_cast<unsigned>(p.plant)];
        const auto phase = p.ready ? 2u : p.growth_ms >= duration / 2 ? 1u : 0u;
        const auto id = PlantId(p.plant, phase);
        if (id != plant_ids[i]) {
            lv_image_set_src(plants[i], Get(id).image);
            plant_ids[i] = id;
        }
        Place(plants[i], id, PlotX(s, i), spring_ui::kSoilY);
        Place(planters[i], Id::Planter, PlotX(s, i) - 51, i == 3 ? 383 : 389);
    }
    for (unsigned i = 0; i < 8; ++i)
        Visible(facilities[i], i != 4 && (s.economy.built & (1u << i)) != 0);
    const bool young = s.tree_stage == 0;
    Place(facilities[7], Id::Facility_nest, 370, young ? 145 : 127);
    Place(tree_home, Id::TreeHome, 394, young ? 221 : 206);
    Visible(tree_home, (s.economy.built & 128u) != 0);
    Place(animals[0], Id::Animal_bird, 411, young ? 160 : 142);
    Place(animals[1], Id::Animal_mouse, 428, young ? 267 : 252);
    bool recent_visit = false;
    for (unsigned i = 0; i < s.event_count; ++i)
        if (s.events[i].type == spring::Event::BirdVisited &&
            s.game_ms / 1000 >= s.events[i].second && s.game_ms / 1000 - s.events[i].second < 20)
            recent_visit = true;
    const bool visiting = s.economy.bird_visit && s.tree_stage >= 1 &&
                          (recent_visit || (s.game_ms / 1000) % 120 < 20);
    Visible(animals[0], (s.economy.animals & 1) || visiting);
    Visible(animals[1], (s.economy.animals & 2) != 0);
    const auto desired_tree = s.tree_stage >= 2 ? Id::TreeCanopy
                              : s.tree_stage    ? Id::TreeGrown
                                                : Id::TreeYoung;
    if (tree_id != desired_tree) {
        lv_image_set_src(tree, Get(desired_tree).image);
        Place(tree, desired_tree, spring_ui::kTreeX, spring_ui::kTreeGroundY);
        if (tree_id != Id::Count)
            lv_image_cache_drop(Get(tree_id).image);
        tree_id = desired_tree;
    }
    for (auto* object : decor)
        Visible(object, false);
    spring_gardener::Pose pose;
    {
        std::lock_guard<std::mutex> guard(motion_mutex);
        pose = gardener.Get();
    }
    if (now - robot_at >= (quiet ? 250u : 83u)) {
        robot_x = pose.x;
        static constexpr Id animation_base[] = {
            Id::Hover_hover_0,    Id::Hover_takeoff_0,     Id::Hover_rest_0,
            Id::Life_care_0,      Id::Hover_water_0,       Id::Hover_build_0,
            Id::Hover_listen_0,   Id::Hover_speak_0,       Id::Hover_recover_0,
            Id::Hover_squint_0,   Id::Hover_wave_0,        Id::Life_care_0,
            Id::Life_harvest_0,   Id::Life_inspect_left_0, Id::Life_inspect_plant_0,
            Id::Life_rest_tree_0, Id::Life_watch_window_0};
        static_assert(sizeof(animation_base) / sizeof(animation_base[0]) ==
                      static_cast<unsigned>(spring_gardener::Animation::WatchWindow) + 1);
        const auto id = static_cast<Id>(
            static_cast<unsigned>(animation_base[static_cast<unsigned>(pose.animation)]) +
            pose.frame);
        if (id != robot_id) {
            lv_image_set_src(robot, Get(id).image);
            robot_id = id;
        }
        Place(robot, id, static_cast<int>(pose.x), static_cast<int>(pose.y));
        const int shadow_width = std::clamp(static_cast<int>(40 - (292 - pose.y) * .24f), 26, 40);
        if (lv_obj_get_width(shadow) != shadow_width)
            lv_obj_set_width(shadow, shadow_width);
        lv_obj_set_pos(shadow, static_cast<int>(pose.x) - shadow_width / 2,
                       pose.animation == spring_gardener::Animation::RestTree ? 445 : 361);
        spring_ui::OpacityChanged(shadow,
                                  pose.mode == spring_gardener::Mode::Rest ? LV_OPA_30 : LV_OPA_20);
        robot_at = now;
    }
    const bool receiving = pose.voice == spring_gardener::Voice::Listen;
    const bool broadcasting = pose.voice == spring_gardener::Voice::Speak;
    Visible(voice_speaker, broadcasting);
    Visible(voice_receiver, receiving);
    lv_obj_set_pos(voice_speaker, static_cast<int>(pose.x) + 28, static_cast<int>(pose.y) - 9);
    lv_obj_set_pos(voice_receiver, static_cast<int>(pose.x) - 35, static_cast<int>(pose.y));
    for (unsigned i = 0; i < 3; ++i) {
        auto* arc = voice_marks[i];
        Visible(arc, receiving || broadcasting);
        if (!receiving && !broadcasting)
            continue;
        const int radius = 9 + 4 * i;
        const int center = static_cast<int>(pose.x) + (broadcasting ? 41 : -33);
        lv_obj_set_pos(arc, center - radius, static_cast<int>(pose.y) + 2 - radius);
        const int start = broadcasting ? 315 : 135, end = broadcasting ? 45 : 225;
        if (lv_arc_get_angle_start(arc) != start)
            lv_arc_set_angles(arc, start, end);
        const unsigned phase = (now / (quiet ? 300 : 180)) % 3;
        const unsigned bright = receiving ? 2 - phase : phase;
        const auto opacity = i == bright ? LV_OPA_COVER : LV_OPA_30;
        if (lv_obj_get_style_arc_opa(arc, LV_PART_INDICATOR) != opacity)
            lv_obj_set_style_arc_opa(arc, opacity, LV_PART_INDICATOR);
    }
    for (unsigned i = 0; i < 3; ++i) {
        const bool watering = pose.mode == spring_gardener::Mode::Water && pose.frame == 2;
        Visible(drops[i], watering);
        if (watering) {
            const float t = ((now + i * 170) % 600) / 600.f;
            const float from = robot_x + 24;
            lv_obj_set_pos(drops[i], static_cast<int>(from + (PlotX(s, s.target) - from) * t),
                           static_cast<int>(pose.y + 12 + (spring_ui::kSoilY - pose.y - 12) * t));
        }
    }
    int wet = s.dew_slot;
    if (wet < 0)
        for (unsigned i = 0; i < PlotCount(s); ++i)
            if ((i < 3 ? s.plots[i] : s.economy.fourth).ready &&
                (i < 3 ? s.plots[i] : s.economy.fourth).plant == spring::Plant::Moss) {
                wet = i;
                break;
            }
    Visible(dew, wet >= 0);
    for (auto* channel : channels)
        Visible(channel, wet >= 0 && std::abs(s.dew_path) > .02f);
    if (wet >= 0) {
        const float t = std::abs(s.dew_path), from = PlotX(s, wet), to = s.dew_path > 0 ? 390 : 100;
        const float x = t < .3f ? from : from + (to - from) * std::min(1.f, (t - .3f) / .6f);
        const float y = t < .3f ? 320 + 88 * t / .3f : t < .9f ? 408 : 408 + 14 * (t - .9f) / .1f;
        lv_obj_set_pos(dew, static_cast<int>(x) - 10, static_cast<int>(y));
    }
    const uint32_t resources[] = {s.water, s.energy, s.fiber};
    for (unsigned i = 0; i < 3; ++i) {
        if (have_previous && previous_demo == s.demo && resources[i] > previous_resources[i])
            reward_at[i] = now;
        const bool active = reward_at[i] && now - reward_at[i] < 1000;
        Visible(rewards[i], active);
        if (active) {
            const float t = (now - reward_at[i]) / 1000.f;
            lv_obj_set_pos(rewards[i],
                           static_cast<int>(robot_x + ((i == 0   ? 170
                                                        : i == 1 ? 278
                                                                 : 62) -
                                                       robot_x) *
                                                          t),
                           static_cast<int>(370 - 328 * t));
        }
        previous_resources[i] = resources[i];
    }
    have_previous = true;
    previous_demo = s.demo;
}
}  // namespace spring_art
