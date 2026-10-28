#include "spring_art.h"
#include "spring_touch.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace spring_ui;
static spring::Core game;
static lv_obj_t *modal, *modal_title, *modal_body, *modal_hint, *primary,
    *choices[3], *choice_labels[3], *economy_icon;
static unsigned modal_kind = 0, economy_page = 0, proposal_token = 0,
                selected_call = 0;
enum class Action {
  Propose,
  ConfirmProposal,
  DismissProposal,
  Mute,
  Accept,
  DemoJourney
};
spring::Snapshot GetWorldSnapshot() { return game.Get(); }
bool Send(Action a, unsigned value = 0, unsigned arg = 0, unsigned extra = 0) {
  if (a == Action::Propose)
    return game.Propose(value, arg, extra);
  if (a == Action::ConfirmProposal)
    return game.ConfirmProposal(value);
  if (a == Action::DismissProposal)
    return game.DismissProposal(value);
  if (a == Action::Accept)
    return game.Accept(value);
  return true;
}
void HideModal(lv_event_t *) {
  lv_obj_add_flag(modal, LV_OBJ_FLAG_HIDDEN);
  modal_kind = 0;
}
void Modal(unsigned kind) {
  modal_kind = kind;
  lv_obj_remove_flag(modal, LV_OBJ_FLAG_HIDDEN);
  for (auto *b : choices)
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(primary, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_state(primary, LV_STATE_DISABLED);
  lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
}
void StoryModal(unsigned kind) { Modal(kind); }
#include "spring_economy_ui.inc"
static uint8_t pixels[480 * 480 * 4];
int main(int argc, char **argv) {
  assert(argc == 2);
  lv_init();
  auto *display = lv_display_create(480, 480);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
  lv_display_set_buffers(display, pixels, nullptr, sizeof(pixels),
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display,
                          [](lv_display_t *d, const lv_area_t *, uint8_t *) {
                            lv_display_flush_ready(d);
                          });
  game.storage_ok = true;
  game.EnableEconomy(true);
  game.Wake();
  game.s.fiber = 60;
  game.s.economy.parts = 20;
  spring_art::Create(lv_screen_active());
  spring_art::Update(game.Get(), 0, false);
  modal = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(modal);
  lv_obj_set_pos(modal, kDialog.x, kDialog.y);
  lv_obj_set_size(modal, kDialog.width, kDialog.height);
  lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(modal, lv_color_hex(0xF2ECD8), 0);
  auto label = [&](int x, int y, int w) {
    auto *l = lv_label_create(modal);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    return l;
  };
  modal_title = label(20, 16, 280);
  modal_body = label(20, 54, 424);
  modal_hint = label(20, 188, 424);
  auto button = [&](Rect r) {
    auto *b = Target(modal, r);
    auto *l = lv_label_create(b);
    lv_label_set_text(l, "");
    Passive(l);
    return b;
  };
  for (unsigned i = 0; i < 3; ++i) {
    choices[i] = button({14 + 117 * static_cast<int>(i), 211, 111, 44});
    choice_labels[i] = lv_obj_get_child(choices[i], 0);
  }
  primary = button({17, 272, 150, 46});
  auto *back = button({240,322,210,56});
  LayoutDialog(modal,primary,back,choices);
  assert(kDialog.y>=72 && kDialog.x+kDialog.width<=480 && kDialog.y+kDialog.height<=448);
  for(auto *b : {choices[0],choices[1],choices[2],primary,back}) {
    lv_obj_update_layout(modal);
    assert(lv_obj_get_height(b)>=56);
    for(int dx : {6,int(lv_obj_get_width(b))/2,int(lv_obj_get_width(b))-7}) {
      lv_point_t p{kDialog.x+lv_obj_get_x(b)+dx,kDialog.y+lv_obj_get_y(b)+28};
      assert(lv_indev_search_obj(lv_layer_top(),&p)==b);
    }
  }
  economy_icon = spring_art::Image(modal, spring_art::Id::Icon_food, 300, 8);
  for (unsigned kind : {7u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u}) {
    EconomyModal(kind);
    const unsigned count = kind == 10   ? 9
                           : kind == 11 ? 6
                           : kind == 12 ? 4
                           : kind == 13 ? 2
                           : kind == 15 ? 8
                           : kind == 14 ? 5
                                        : 1;
    for (economy_page = 0; economy_page < count; ++economy_page) {
      RenderEconomyModal(game.Get());
      lv_obj_update_layout(modal);
      lv_refr_now(display);
      for (auto *b : choices) {
        if (lv_obj_has_flag(b, LV_OBJ_FLAG_HIDDEN)) continue;
        auto x = lv_obj_get_x(b), y = lv_obj_get_y(b);
        lv_point_t p{kDialog.x + x + 20, kDialog.y + y + 20};
        assert(lv_indev_search_obj(lv_layer_top(), &p) == b);
      }
    }
  }
  for (unsigned kind = 1; kind <= 7; ++kind) {
    if (kind == 5) {
      game.s.economy.journey = 1;
      game.s.economy.journey_id = 1;
    }
    assert(game.Propose(kind, kind == 5 ? 1 : 0, 0));
    EconomyModal(8);
    RenderEconomyModal(game.Get());
    lv_refr_now(display);
    assert(proposal_token == game.s.economy.proposal.token);
  }
  // Reproduce built-page -> next unbuilt-page: view must become clickable again.
  EconomyModal(10);economy_page=0;game.s.economy.built=1;
  RenderEconomyModal(game.Get());assert(lv_obj_has_state(primary,LV_STATE_DISABLED));
  EconomyChoice(1);RenderEconomyModal(game.Get());
  assert(economy_page==1&&!lv_obj_has_state(primary,LV_STATE_DISABLED));
  EconomyConfirm();assert(game.s.economy.proposal.kind==1&&game.s.economy.proposal.target==1);
  game.s.economy.built=0;
  EconomyModal(10);
  economy_page = 0;
  EconomyConfirm();
  assert(game.s.economy.proposal.kind == 1);
  EconomyModal(8);
  RenderEconomyModal(game.Get());
  assert(std::string(lv_label_get_text(lv_obj_get_child(primary, 0))) ==
         "确认建造");
  assert(lv_obj_has_flag(choices[1], LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(choices[2], LV_OBJ_FLAG_HIDDEN));
  auto before = game.s.fiber;
  EconomyConfirm();
  assert(game.s.fiber == before - 9);
  assert(game.s.economy.construction == 0);
  EconomyModal(10);
  RenderEconomyModal(game.Get());
  assert(std::string(lv_label_get_text(modal_body)).find("还需约120秒") !=
         std::string::npos);
  assert(game.Propose(1, 7));
  EconomyModal(8);
  game.s.fiber = 0;
  game.s.economy.construction = 255;
  game.s.economy.construction_ms = 0;
  RenderEconomyModal(game.Get());
  assert(lv_obj_has_state(primary, LV_STATE_DISABLED));
  assert(std::string(lv_label_get_text(modal_hint)).find("繁茂树冠") != std::string::npos);
  game.s.tree_stage = game.s.story_stage = 2;
  RenderEconomyModal(game.Get());
  assert(std::string(lv_label_get_text(modal_hint)).find("藤纤维不足") !=
         std::string::npos);
  std::ofstream out(std::string(argv[1]) + "/economy-ui.ppm", std::ios::binary);
  out << "P6\n480 480\n255\n";
  for (unsigned i = 0; i < 480 * 480; ++i) {
    char rgb[] = {char(pixels[i * 4 + 2]), char(pixels[i * 4 + 1]),
                  char(pixels[i * 4])};
    out.write(rgb, 3);
  }
  lv_deinit();
  std::cout << "Economy UI: all record/facility/catalog/animal/priority pages "
               "render, hit targets and proposal-confirm transaction passed\n";
}
