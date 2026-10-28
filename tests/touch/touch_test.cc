#include "game/touch_buffer.h"
#include "spring_touch.h"
#include "spring_subtitle.h"
#include "spring_font.h"
#include <cassert>
#include <iostream>
using namespace spring_ui;
lv_obj_t *At(lv_obj_t *root, int x, int y) {
  lv_point_t p{x, y};
  return lv_indev_search_obj(root, &p);
}
int main() {
  lv_init();
  auto *display = lv_display_create(480, 480);
  static uint8_t buffer[480 * 24 * 4];
  lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display,
                          [](lv_display_t *d, const lv_area_t *, uint8_t *) {
                            lv_display_flush_ready(d);
                          });
  assert(!HasGameChinese(nullptr));
  assert(!HasGameChinese(LV_FONT_DEFAULT));
  lv_font_t checked_font = *LV_FONT_DEFAULT;
  checked_font.get_glyph_dsc = +[](const lv_font_t*, lv_font_glyph_dsc_t* d,
                             uint32_t code, uint32_t) {
    d->box_w = d->box_h = 20;
    d->is_placeholder = false;
    return code >= 0x4E00;
  };
  assert(HasGameChinese(&checked_font));
  checked_font.get_glyph_dsc = +[](const lv_font_t*, lv_font_glyph_dsc_t* d,
                             uint32_t, uint32_t) {
    d->box_w = d->box_h = 20;
    d->is_placeholder = true;
    return true;
  };
  assert(!HasGameChinese(&checked_font));
  // A 46-line-height Chinese font at scale 166 must stay inside the button.
  auto* large_button = Target(lv_screen_active(), {14, 296, 210, 56});
  auto* button_label = lv_label_create(large_button);
  lv_font_t large_font = *LV_FONT_DEFAULT;
  large_font.line_height = 46;
  lv_obj_set_style_text_font(button_label, &large_font, 0);
  lv_obj_set_style_transform_scale_x(button_label, 166, 0);
  lv_obj_set_style_transform_scale_y(button_label, 166, 0);
  lv_obj_set_width(button_label, 186 * 256 / 166);
  PlaceButtonText(large_button, button_label);
  lv_obj_update_layout(lv_screen_active());
  assert(lv_obj_get_x(button_label) == 12);
  assert(lv_obj_get_y(button_label) == 13);
  assert(lv_obj_get_width(button_label) * 166 / 256 <= 186);
  assert(lv_obj_get_style_text_align(button_label, LV_PART_MAIN) == LV_TEXT_ALIGN_CENTER);
  lv_obj_delete(large_button);
  // Moving the subtitle to the top layer loses the screen's inherited CJK font.
  lv_font_t cjk = *LV_FONT_DEFAULT, replacement = *LV_FONT_DEFAULT;
  lv_obj_set_style_text_font(lv_screen_active(), &cjk, 0);
  auto* subtitle_bar = lv_obj_create(lv_screen_active());
  auto* subtitle = lv_label_create(subtitle_bar);
  assert(lv_obj_get_style_text_font(subtitle, LV_PART_MAIN) == &cjk);
  lv_obj_set_parent(subtitle_bar, lv_layer_top());
  assert(lv_obj_get_style_text_font(subtitle, LV_PART_MAIN) != &cjk);
  BindSubtitleFont(subtitle, &cjk);
  assert(lv_obj_get_style_text_font(subtitle, LV_PART_MAIN) == &cjk);
  assert(lv_color_eq(lv_obj_get_style_text_color(subtitle,LV_PART_MAIN), lv_color_hex(0xFFFFFF)));
  // Asset/theme font replacement must rebind the label before old font release.
  BindSubtitleFont(subtitle, &replacement);
  assert(lv_obj_get_style_text_font(subtitle, LV_PART_MAIN) == &replacement);
  lv_obj_delete(subtitle_bar);
  lv_obj_set_style_text_font(lv_screen_active(), LV_FONT_DEFAULT, 0);
  auto *probe = lv_label_create(lv_screen_active());
  lv_label_set_text(probe, "unchanged");
  lv_obj_update_layout(lv_screen_active());
  lv_refr_now(display);
  int invalidations = 0;
  const auto invalidation_event =
      +[](lv_event_t *e) { ++*static_cast<int *>(lv_event_get_user_data(e)); };
  lv_display_add_event_cb(display, invalidation_event, LV_EVENT_INVALIDATE_AREA,
                          &invalidations);
  for (int i = 0; i < 100; ++i) {
    assert(!TextChanged(probe, "unchanged"));
    assert(!TextFormat(probe, "%s", "unchanged"));
  }
  assert(invalidations == 0);
  assert(TextChanged(probe, "changed"));
  lv_obj_update_layout(lv_screen_active());
  lv_refr_now(display);
  assert(std::strcmp(lv_label_get_text(probe), "changed") == 0);
  assert(invalidations > 0);
  lv_display_remove_event_cb_with_user_data(display, invalidation_event,
                                            &invalidations);
  lv_obj_delete(probe);
  auto *screen = lv_screen_active();
  auto *stage = lv_obj_create(screen);
  lv_obj_remove_style_all(stage);
  lv_obj_set_size(stage, 480, 480);
  Passive(stage);
  auto *legacy_pot = Target(stage, {208, 268, 52, 51});
  lv_obj_set_ext_click_area(legacy_pot, 16);
  lv_obj_update_layout(screen);
  assert(At(screen, 234, 221) != legacy_pot);
  assert(At(screen, 234, 287) == legacy_pot);
  lv_obj_delete(legacy_pot);
  lv_obj_t *plots[3];
  for (int i = 0; i < 3; ++i)
    plots[i] = Target(stage, kPlots[i]);
  auto *tree = Target(stage, kTree);
  auto *hud = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(hud);
  lv_obj_set_size(hud, 480, 480);
  Passive(hud);
  lv_obj_t *toolbar[4];
  for (int i = 0; i < 4; ++i) {
    toolbar[i] = Target(hud, kToolbar[i]);
    auto *label = lv_label_create(toolbar[i]);
    lv_label_set_text(label, "button");
    Passive(label);
  }
  auto *legacy_card = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(legacy_card);
  lv_obj_set_pos(legacy_card, 53, 78);
  lv_obj_set_size(legacy_card, 374, 333);
  Passive(legacy_card);
  lv_obj_update_layout(lv_layer_top());
  assert(At(lv_layer_top(), 60, 94) == toolbar[0]);
  lv_obj_delete(legacy_card);
  auto *backdrop = Target(lv_layer_top(), {0, 0, 480, 480});
  auto *dialog = Target(backdrop, {53, 78, 374, 333});
  auto *confirm = Target(dialog, {17, 272, 150, 46});
  auto *back = Target(dialog, {203, 272, 150, 46});
  DialogVisible(backdrop, dialog, false);
  lv_obj_update_layout(screen);
  lv_obj_update_layout(lv_layer_top());
  for (int i = 0; i < 3; ++i) {
    const auto r = kPlots[i];
    assert(At(screen, r.x + r.width / 2, 221) ==
           plots[i]); // leaf, not just the pot
    assert(At(screen, r.x + r.width / 2, 361) == plots[i]); // plant caption
  }
  for (int i = 0; i < 4; ++i) {
    const auto r = kToolbar[i];
    assert(At(lv_layer_top(), r.x + r.width / 2, r.y + 8) == toolbar[i]);
    assert(At(lv_layer_top(), r.x + r.width / 2, r.y + r.height - 3) ==
           toolbar[i]);
  }
  assert(At(screen, 438, 263) == tree);
  DialogVisible(backdrop, dialog, true);
  lv_obj_update_layout(lv_layer_top());
  assert(At(lv_layer_top(), 60, 94) ==
         dialog); // obscured toolbar cannot receive input
  assert(At(lv_layer_top(), 25, 300) ==
         backdrop); // outside card cannot reach plants
  assert(At(lv_layer_top(), 200, 200) == dialog); // empty card blocks the scene
  assert(At(lv_layer_top(), 125, 372) == confirm);
  assert(At(lv_layer_top(), 322, 372) == back);
  DialogVisible(backdrop, dialog, false);
  assert(At(lv_layer_top(), 60, 94) == toolbar[0]);
  // Close a modal with several bounce edges already queued: cancellation must
  // discard all of them and report a reset, never a successful new down/up pair.
  spring::TouchBuffer stale;
  stale.Push({330,372,true,false,false,1000});
  stale.Push({330,372,false,false,false,2000});
  stale.Push({330,372,true,false,false,3000});
  stale.Cancel(4000);
  spring::TouchSample cancelled;
  assert(stale.Pop(cancelled) && cancelled.reset && !cancelled.pressed);
  assert(!stale.Pending());
  assert(!stale.Pop(cancelled) && !cancelled.pressed && !cancelled.reset);
  spring::TouchTransitionGuard guard;
  guard.Begin(1000000);
  assert(guard.Blocked(true,false,1010000));
  assert(guard.Blocked(true,true,1100000)); // Contact bounce resets release timer.
  assert(guard.Blocked(true,false,1150000));
  assert(guard.Blocked(false,false,1260000)); // Missing frame is not a release.
  assert(guard.Blocked(true,true,1400000)); // A held finger never rearms input.
  assert(guard.Blocked(true,false,1450000));
  assert(guard.Blocked(true,false,1570000)); // Consume the stable release itself.
  assert(!guard.Blocked(true,true,1580000)); // Only a fresh gesture is accepted.
  spring::TouchBuffer reports;
  spring::TouchSample sample;
  assert(!reports.Pop(sample) && !sample.pressed);
  reports.Push({60, 94, true, false, false, 1000});
  reports.Push({60, 94, false, false, false, 21000});
  assert(reports.Pop(sample) && sample.pressed && sample.sampled_us == 1000);
  assert(reports.Pop(sample) && !sample.pressed && sample.sampled_us == 21000);
  // A quick tap occurring entirely between UI reads must produce exactly one
  // click.
  auto *input = lv_indev_create();
  lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
  lv_indev_set_user_data(input, &reports);
  lv_indev_set_read_cb(input, [](lv_indev_t *input, lv_indev_data_t *data) {
    auto *buffer =
        static_cast<spring::TouchBuffer *>(lv_indev_get_user_data(input));
    spring::TouchSample s;
    buffer->Pop(s);
    if (s.reset) lv_indev_reset(input, nullptr);
    data->point = {s.x, s.y};
    data->state = s.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = buffer->Pending() > 0;
  });
  int clicks = 0;
  lv_obj_add_event_cb(
      toolbar[0],
      [](lv_event_t *e) { ++*static_cast<int *>(lv_event_get_user_data(e)); },
      LV_EVENT_CLICKED, &clicks);
  reports.Push({60, 94, true, false, false, 40000});
  reports.Push({60, 94, false, false, false, 60000});
  lv_indev_read(input);
  assert(clicks == 1);
  lv_indev_read(input);
  assert(clicks == 1);
  // The robot can begin work between down and up. Its invitation must stay
  // under the finger and deliver exactly one click, without opening ordinary chat.
  auto* bubble = lv_obj_create(hud);
  lv_obj_remove_style_all(bubble);
  lv_obj_set_size(bubble, 64, 48);
  ConfigureInvitationBubble(bubble);
  PlaceInvitationBubble(bubble, true, true, 220, 180);
  auto* dots = lv_label_create(bubble);
  lv_label_set_text(dots, "...");
  Passive(dots);
  int bubble_clicks = 0;
  lv_obj_add_event_cb(bubble, [](lv_event_t* e) {
    ++*static_cast<int*>(lv_event_get_user_data(e));
  }, LV_EVENT_CLICKED, &bubble_clicks);
  lv_obj_update_layout(lv_layer_top());
  assert(At(lv_layer_top(), 215, 200) == bubble); // Expanded finger-sized hit area.
  reports.Push({245, 200, true, false, false, 61000});
  lv_indev_read(input);
  assert(lv_obj_has_state(bubble, LV_STATE_PRESSED));
  PlaceInvitationBubble(bubble, true, false, 300, 260); // Work starts and pose moves.
  assert(!lv_obj_has_flag(bubble, LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_get_x(bubble) == 220 && lv_obj_get_y(bubble) == 180);
  reports.Push({248, 202, false, false, false, 65000});
  lv_indev_read(input);
  assert(bubble_clicks == 1);
  PlaceInvitationBubble(bubble, true, false, 300, 260);
  assert(lv_obj_has_flag(bubble, LV_OBJ_FLAG_HIDDEN)); // Idle-only outside a tap.
  PlaceInvitationBubble(bubble, false, true, 220, 180);
  assert(lv_obj_has_flag(bubble, LV_OBJ_FLAG_HIDDEN)); // Expired invitation stays gone.
  lv_obj_delete(bubble);
  // A return control directly over a scene button must consume queued bounce.
  DialogVisible(backdrop, dialog, true);
  auto* close_overlay = Target(backdrop, {12,74,102,42});
  struct CloseCase { spring::TouchBuffer* queue; lv_obj_t* backdrop; lv_obj_t* dialog; int count; } closing{&reports,backdrop,dialog,0};
  lv_obj_add_event_cb(close_overlay, [](lv_event_t* event) {
    auto* c=static_cast<CloseCase*>(lv_event_get_user_data(event));
    ++c->count; c->queue->Cancel(90000);
    lv_indev_wait_release(lv_indev_active());
    DialogVisible(c->backdrop,c->dialog,false);
  }, LV_EVENT_CLICKED, &closing);
  reports.Push({60,94,true,false,false,70000});
  reports.Push({60,94,false,false,false,75000});
  reports.Push({60,94,true,false,false,80000});
  reports.Push({60,94,false,false,false,85000});
  lv_obj_update_layout(lv_layer_top());
  lv_indev_read(input); lv_indev_read(input);
  assert(closing.count == 1 && clicks == 1);
  lv_obj_delete(close_overlay);
  // Missing data cannot split a held finger into clicks on overlapping layers.
  spring::TouchFrameFilter filter;
  auto feed = [&](bool valid, bool down, uint64_t time) {
    spring::TouchSample value;
    if (filter.Update(valid, down, 60, 94, time, value)) reports.Push(value);
    lv_indev_read(input);
  };
  feed(true, true, 100000);
  feed(false, false, 170000); // old 60ms timeout would fabricate a click
  assert(clicks == 1);
  feed(false, false, 400000); // cancel, never CLICKED
  assert(clicks == 1);
  feed(true, true, 410000); // same finger returns, still quarantined
  feed(true, false, 420000);
  assert(clicks == 1);
  feed(true, true, 430000);
  feed(true, false, 450000);
  feed(true, false, 500000); // stable release completes one tap
  assert(clicks == 2); // next real tap works immediately
  spring::TouchFrameFilter bounce;
  spring::TouchSample checked;
  assert(bounce.Update(true,true,300,350,1000000,checked));
  assert(!bounce.Update(true,false,0,0,1010000,checked));
  assert(bounce.Update(true,true,300,350,1030000,checked) && checked.pressed);
  assert(!bounce.Update(true,false,0,0,1040000,checked));
  assert(bounce.Update(true,false,0,0,1090000,checked) && !checked.pressed);
  assert(checked.x==300 && checked.y==350);
  spring::TapGuard taps;
  assert(taps.Accept(1000000));assert(!taps.Accept(1060000));
  assert(!taps.Accept(1200000));assert(taps.Accept(1240000));
  int mode_requests = 0;
  lv_obj_add_event_cb(toolbar[3], [](lv_event_t *e) {
    ++*static_cast<int *>(lv_event_get_user_data(e));
  }, LV_EVENT_LONG_PRESSED, &mode_requests);
  reports.Push({410, 94, true, false, false, 500000});
  reports.Push({410, 94, false, false, false, 520000});
  lv_indev_read(input);
  assert(mode_requests == 0); // quick tap at the overlapping return position
  reports.Push({410, 94, true, false, false, 600000});
  lv_indev_read(input);
  lv_tick_inc(600);
  lv_indev_read(input);
  assert(mode_requests == 1);
  reports.Push({410, 94, false, false, false, 1300000});
  lv_indev_read(input);
  spring::TouchBuffer moving;
  assert(moving.Push({1, 1, true, false, false, 1}));
  assert(moving.Push({2, 2, true, false, false, 2}));
  assert(moving.Push({3, 3, true, false, false, 3}));
  assert(moving.Pending() == 2);
  assert(moving.Pop(sample) && sample.edge && sample.x == 1);
  assert(moving.Pop(sample) && !sample.edge && sample.x == 3);
  assert(!moving.Push({3, 3, true, false, false, 4}));
  for (int i = 0; i < 40; ++i)
    reports.Push(
        {100, 100, bool(i % 2), false, false, static_cast<uint64_t>(i)});
  assert(reports.Overflows() > 0);
  assert(reports.Pop(sample) && sample.reset && !sample.pressed);
  while (reports.Pending())
    reports.Pop(sample);
  reports.Pop(sample);
  assert(!sample.reset);
  lv_indev_delete(input);
  std::cout << "LVGL hit tests passed: foliage, labels, toolbar edges, tree, "
               "modal shield, confirm/back, close recovery, unchanged-text "
               "redraw suppression\n";
  lv_deinit();
}
