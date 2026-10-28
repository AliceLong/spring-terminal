#include "spring_art.h"
#include "spring_touch.h"
#include "spring_scene_touch.h"
#include "story_font.h"
#include "game/spring_social.h"
#include "game/spring_agency.h"
#include <atomic>
#include <mutex>
#include <fstream>
#include <iostream>
using namespace spring_ui;
static spring::Core game;
static lv_obj_t *modal, *modal_title, *modal_body, *modal_hint, *primary,
    *choices[3], *choice_labels[3], *back, *economy_icon;
static unsigned modal_kind = 0, economy_page = 0, proposal_token = 0, selected_call = 0;
static unsigned archive_ui_requested = 0;
static unsigned collection_mode_requests = 0;
enum class CommunicationSource { None, Player, Life, Story };
std::atomic<CommunicationSource> communication_source{CommunicationSource::None};
std::atomic<unsigned> communication_archive{255}, active_call{99};
bool communication_closing = false;
std::mutex social_mutex;
spring::SocialState social_public;
spring::AgencyState agency_public;
bool QueueWishResponse(uint32_t token, unsigned choice, bool) {
    if(choice==1) return spring::StageWishApproval(game,agency_public,token,spring::MemoryActor::Player);
    if(token != agency_public.current.token || !spring::WishPending(agency_public)) return false;
    spring::FinishWish(agency_public, choice==2 ? spring::WishStatus::Deferred : spring::WishStatus::Declined); return true;
}
lv_obj_t *archive_scroll, *archive_content, *archive_text;
enum class Action { Propose, ConfirmProposal, DismissProposal, Mute, Accept, DemoJourney, Mode };
bool Send(Action a, unsigned value = 0, unsigned arg = 0, unsigned extra = 0) {
    if (a == Action::Mode) { ++collection_mode_requests; return true; }
    if (a == Action::Propose) return game.Propose(value, arg, extra);
    if (a == Action::ConfirmProposal) return game.ConfirmProposal(value);
    if (a == Action::DismissProposal) return game.DismissProposal(value);
    return true;
}
void StoryModal(unsigned);

enum { kDeviceStateIdle, kDeviceStateSpeaking, kDeviceStateListening };
struct Application {
  static Application &GetInstance() {
    static Application a;
    return a;
  }
  int GetDeviceState() { return kDeviceStateIdle; }
  template <class T> void Schedule(T f) { f(); }
  void InvokeDeviceEvent(const char *) {}
};
spring::Snapshot GetWorldSnapshot() { return game.Get(); }
static unsigned collection_practice_requests = 0;
void StartCollectionPractice(const spring::Snapshot& s) {
    assert(s.demo);
    ++collection_practice_requests;
}
bool QueueStoryAction(unsigned verb, unsigned target, unsigned value,
                      unsigned token, const char *text, bool) {
  switch (static_cast<spring::StoryVerb>(verb)) {
  case spring::StoryVerb::Accept:
  case spring::StoryVerb::Inspect:
    return game.AcceptStory(target);
  case spring::StoryVerb::Complete:
    return game.CompleteStory(target, value);
  case spring::StoryVerb::Archive: {
    const bool ok=game.ReadStoryArchive(target);
    if (ok) archive_ui_requested=target+1;
    return ok;
  }
  case spring::StoryVerb::Presented:
    return game.StoryPresented(target);
  case spring::StoryVerb::ArchiveSeen:
    return game.StoryArchiveSeen(target);
  case spring::StoryVerb::OfferFlower:
    return game.OfferStoryFlower();
  case spring::StoryVerb::ApproveFlower:
    return game.ApproveStoryFlower(token);
  case spring::StoryVerb::OfferName:
    return game.ProposeStoryName(target, text);
  case spring::StoryVerb::ConfirmName:
    return game.ConfirmStoryName(token);
  case spring::StoryVerb::InviteSeat:
    return game.InviteLife(3, 0, spring::MemoryActor::Player);
  case spring::StoryVerb::Pause:
    return game.PauseStory(target);
  default:
    return false;
  }
}
void HideModal(lv_event_t *) {
  lv_obj_add_flag(modal, LV_OBJ_FLAG_HIDDEN);
  modal_kind = 0;
}
void Modal(unsigned kind) {
  modal_kind = kind;
  if (economy_icon) lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(modal, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_state(primary, LV_STATE_DISABLED);
  for (auto *b : choices)
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
  if (archive_scroll) {
    if (kind == 19) {
      lv_obj_add_flag(modal_body, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(archive_scroll, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(modal_body, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(archive_scroll, LV_OBJ_FLAG_HIDDEN);
    }
  }
  LayoutDialog(modal,primary,back,choices);
  lv_obj_set_pos(modal_hint,20,188);
  if (kind == 27) LayoutCommunicationCard(modal,modal_hint,back,communication_source==CommunicationSource::Life&&active_call==98?choices:nullptr);
}
#include "spring_economy_ui.inc"
#include "spring_story_ui.inc"
static uint8_t pixels[480 * 480 * 4];
void capture(const std::string &file) {
  std::ofstream f(file, std::ios::binary);
  f << "P6\n480 480\n255\n";
  for (unsigned i = 0; i < 480 * 480; ++i) {
    const char b[] = {char(pixels[i * 4 + 2]), char(pixels[i * 4 + 1]),
                      char(pixels[i * 4])};
    f.write(b, 3);
  }
}
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
  StoryFont device_font(
      "vendor/xiaozhi-esp32/managed_components/78__xiaozhi-fonts/cbin/"
      "font_noto_sans_common_30_4.bin");
  unsigned glyphs = 0;
  auto check = [&](const char *text) {
    for (size_t i = 0; text[i];) {
      uint32_t c = static_cast<unsigned char>(text[i++]);
      if (c >= 0xe0) {
        c &= 15;
        c = (c << 6) | (text[i++] & 63);
        c = (c << 6) | (text[i++] & 63);
      } else if (c >= 0xc0) {
        c &= 31;
        c = (c << 6) | (text[i++] & 63);
      }
      if (c < 32)
        continue;
      lv_font_glyph_dsc_t d{};
      assert(device_font.font.get_glyph_dsc(&device_font.font, &d, c, 0) &&
             !d.is_placeholder);
      ++glyphs;
    }
  };
  for (const auto &spec : spring::kStorySpecs) {
    check(spec.title);
    check(spec.opening);
    check(spec.detail);
  }
  for (auto *t : spring::kStoryArchiveTexts)
    check(t);
  game.storage_ok = true;
  game.EnableEconomy(true);
  game.EnableMemory(1, 42);
  game.EnableStory();
  game.Wake();
  game.TickStory(100);
  spring_art::Create(lv_screen_active());
  spring_art::Update(game.Get(), 100, false);
  modal = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(modal);
  lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(modal, lv_color_hex(0xf2ecd8), 0);
  auto label = [&](lv_obj_t *parent, int x, int y, int w, int size) {
    auto *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w * 46 / size);
    lv_obj_set_style_text_font(l, &device_font.font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x293a28), 0);
    lv_obj_set_style_transform_scale_x(l, size * 256 / 46, 0);
    lv_obj_set_style_transform_scale_y(l, size * 256 / 46, 0);
    lv_obj_set_style_transform_pivot_x(l, 0, 0);
    lv_obj_set_style_transform_pivot_y(l, 0, 0);
    return l;
  };
  modal_title = label(modal, 20, 14, 350, 26);
  modal_body = label(modal, 20, 54, 424, 24);
  archive_scroll = lv_obj_create(modal);
  lv_obj_remove_style_all(archive_scroll);
  lv_obj_set_pos(archive_scroll,20,52);
  lv_obj_set_size(archive_scroll,424,128);
  lv_obj_set_scroll_dir(archive_scroll,LV_DIR_VER);
  archive_content=lv_obj_create(archive_scroll);
  lv_obj_remove_style_all(archive_content);
  lv_obj_remove_flag(archive_content,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(archive_content,0,0);
  lv_obj_set_size(archive_content,422,1);
  archive_text = label(archive_content,2,2,398,22);
  modal_hint = label(modal, 20, 188, 424, 20);
  auto button = [&]() {
    auto *b = Target(modal, {0, 0, 100, 56});
    label(b, 12, 10, 100, 24);
    return b;
  };
  for (unsigned i = 0; i < 3; ++i) {
    choices[i] = button();
    choice_labels[i] = lv_obj_get_child(choices[i], 0);
  }
  primary = button();
  back = button();
  LayoutDialog(modal, primary, back, choices);
  lv_label_set_text(lv_obj_get_child(back, 0), "返回");
  economy_icon = spring_art::Image(modal, spring_art::Id::Icon_food, 388, 8);
  game.s.story.archives = 127;
  game.s.story.available = (1u << spring::kStoryCount) - 1;
  game.s.story.seed = 1;
  game.s.story.seed_choice = 1;
  game.s.story.completed |= 1u << 10;
  game.OfferStoryFlower();
  game.ProposeStoryName(2, "小芽");
  unsigned pages = 0;
  for (unsigned kind = 17; kind <= 28; ++kind) {
    StoryModal(kind);
    if (kind == 24)
      story_selected = 8;
    RenderStoryModal(game.Get());
    check(lv_label_get_text(modal_title));
    check(lv_label_get_text(modal_body));
    check(lv_label_get_text(modal_hint));
    if (kind == 19) check(lv_label_get_text(archive_text));
    for (auto* label : choice_labels) check(lv_label_get_text(label));
    lv_obj_update_layout(modal);
    lv_refr_now(display);
    assert(lv_obj_get_y(modal) >= 72);
    for (auto *b : choices) {
      if (lv_obj_has_flag(b, LV_OBJ_FLAG_HIDDEN))
        continue;
      lv_point_t p{kDialog.x + lv_obj_get_x(b) + 20,
                   kDialog.y + lv_obj_get_y(b) + 20};
      assert(lv_indev_search_obj(lv_layer_top(), &p) == b);
    }
    capture(std::string(argv[1]) + "/story-ui-" + std::to_string(kind) +
            ".ppm");
    ++pages;
  }
  // The hardware-rejected experiment cannot open or start in either save mode.
  {
    static_assert(!spring_ui::kCollectionPracticeEnabled);
    auto saved=game;
    for (bool demo : {false,true}) {
      game.demo=demo;
      StoryModal(17); RenderStoryModal(game.Get());
      assert(lv_obj_has_flag(primary,LV_OBJ_FLAG_HIDDEN));
      StoryConfirm(); assert(modal_kind==17);
      const auto calls=collection_mode_requests;
      StoryModal(28); StoryConfirm();
      assert(collection_mode_requests==calls && !collection_practice_requests);
    }
    game=saved;
  }
  // The real shared modal exposes three distinct responses; opening is no approval.
  {
    auto saved=game;
    game=spring::Core{};game.storage_ok=true;game.EnableEconomy(true);game.EnableMemory(11,22);game.Wake();
    game.s.story.enabled=1;game.s.story.language=1;game.s.story.active=spring::kNoStory;
    game.s.water=game.s.energy=40;for(auto& p:game.s.plots){p.ready=false;p.growth_ms=p.mature_ms=0;}
    game.s.life.cooldown=0;agency_public=spring::EmptyAgency(game.Get());
    assert(spring::OfferWish(game.Get(),agency_public,3,0));assert(spring::PresentWish(agency_public,agency_public.current.token));
    communication_source=CommunicationSource::Life;active_call=98;
    StoryModal(27);RenderStoryModal(game.Get());lv_obj_update_layout(modal);lv_refr_now(display);
    assert(!game.LifeActive());
    for(auto* b:choices){assert(!lv_obj_has_flag(b,LV_OBJ_FLAG_HIDDEN));lv_point_t point{lv_obj_get_x(modal)+lv_obj_get_x(b)+20,lv_obj_get_y(modal)+lv_obj_get_y(b)+20};assert(lv_indev_search_obj(lv_layer_top(),&point)==b);}
    check(lv_label_get_text(modal_body));for(auto* label:choice_labels)check(lv_label_get_text(label));
    capture(std::string(argv[1])+"/agency-wish-presented.ppm");
    StoryChoice(1);assert(!game.LifeActive()&&agency_public.current.status==unsigned(spring::WishStatus::Deferred));
    communication_source=CommunicationSource::None;active_call=99;game=saved;
  }
  for (unsigned id = 0; id < 7; ++id) {
    story_doc = id;
    StoryModal(19);
    for (unsigned p = 0; p < 2; ++p) {
      story_doc_page = p;
      RenderStoryModal(game.Get());
      lv_obj_update_layout(modal);
      lv_obj_scroll_to_y(archive_scroll,p ? 10000 : 0,LV_ANIM_OFF);
      lv_refr_now(display);
      capture(std::string(argv[1]) + "/archive-" + std::to_string(id) + "-" +
              std::to_string(p) + ".ppm");
      ++pages;
    }
  }
  // Repeatedly exercise the actual nested record-instrument handlers, including
  // StoryChoice -> StorySummary and StoryConfirm -> StoryChoice.
  for (unsigned cycle = 0; cycle < 200; ++cycle) {
    StoryModal(17);
    StoryChoice(0);
    assert(modal_kind == 10);
    EconomyModal(10);
    economy_page = 8;
    EconomyConfirm();
    assert(modal_kind == 2);
    StoryModal(17);
    StoryChoice(2);
    assert(modal_kind == 25);
    StoryChoice(1);
    assert(modal_kind == 18);
    StoryConfirm();
    assert(modal_kind == 18 && archive_ui_requested);
    story_doc=archive_ui_requested-1;
    archive_ui_requested=0;
    StoryModal(19); // The real worker signals UI only after successful persistence.
    assert(modal_kind == 19);
    StoryChoice(1);
    StoryConfirm();
    assert(modal_kind == 18);
    StoryModal(17);
    StoryChoice(1);
    assert(modal_kind == 7);
    EconomyChoice(1);
    assert(modal_kind == 21);
    StoryChoice(0);
    assert(modal_kind == 22);
    StoryChoice(2);
    assert(modal_kind == 24);
    RenderStoryModal(game.Get());
    HideModal(nullptr);
  }
  for (unsigned kind : {7u, 10u, 15u}) {
    EconomyModal(kind);
    economy_page = kind == 15 ? 5 : 0;
    RenderEconomyModal(game.Get());
    lv_refr_now(display);
    capture(std::string(argv[1]) + "/economy-font-" + std::to_string(kind) + ".ppm");
  }
  StoryModal(17);
  RenderStoryModal(game.Get());
  assert(lv_obj_has_flag(primary, LV_OBJ_FLAG_HIDDEN));
  assert(std::string(lv_label_get_text(choice_labels[0])) == "建设");
  assert(std::string(lv_label_get_text(choice_labels[1])) == "当前计划");
  assert(std::string(lv_label_get_text(choice_labels[2])) == "收藏");
  StoryChoice(2);
  StoryChoice(0);
  assert(modal_kind == 11);
  EconomyConfirm();
  assert(modal_kind == 25);
  StoryChoice(2);
  assert(modal_kind == 26);
  StoryConfirm();
  assert(modal_kind == 25);
  // Built objects: viewing creates no proposal and spends no resources.
  game.s.economy.built = 255;
  game.s.economy.scrap = 2;
  game.s.energy = 20;
  game.s.economy.proposal = {};
  const auto initial_energy = game.s.energy;
  for (unsigned id = 0; id < 8; ++id) {
    EconomyModal(15);
    economy_page = id;
    RenderEconomyModal(game.Get());
    assert(game.s.energy == initial_energy && !game.s.economy.proposal.kind);
    if (id != 5 && id != 6 && id != 7)
      assert(lv_obj_has_flag(primary, LV_OBJ_FLAG_HIDDEN));
  }
  EconomyModal(15); economy_page = 6; EconomyConfirm();
  assert(modal_kind == 14);
  EconomyModal(15); economy_page = 7; EconomyConfirm();
  assert(modal_kind == 13);
  EconomyModal(15); economy_page = 5; EconomyConfirm();
  assert(game.s.economy.proposal.kind == 6 && game.s.energy == initial_energy);
  EconomyModal(8); EconomyConfirm();
  assert(game.s.energy == initial_energy - 2 && game.s.economy.recycle_ms == 60000);
  // Empty/busy recycling exposes a disabled action rather than charging again.
  EconomyModal(15); economy_page = 5; RenderEconomyModal(game.Get());
  assert(lv_obj_has_state(primary, LV_STATE_DISABLED));
  game.s.economy.recycle_ms = 0;
  game.s.economy.recycle_yield = 0;
  game.s.economy.scrap = 0;
  RenderEconomyModal(game.Get());
  assert(lv_obj_has_state(primary, LV_STATE_DISABLED));
  HideModal(nullptr);
  auto* scene = spring_art::World();
  // Reproduce actual ordering: plots/tree first, facility targets above them.
  lv_obj_t* plots[4]{};
  for (unsigned i = 0; i < 4; ++i)
    plots[i] = Target(scene, {int(i == 3 ? 446 : 130 + 104 * i) - 49, 335, 98, 113});
  auto* tree_hit = Target(scene, kWorldTree);
  lv_obj_t* hits[9]{};
  CreateFacilityTargets(scene, hits, [](lv_event_t* e) {
    EconomyModal(15);
    economy_page = static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  });
  for (unsigned stage = 0; stage < 3; ++stage) {
    game.s.tree_stage = stage;
    for (unsigned built : {0u, 255u}) {
      game.s.economy.built = built;
      spring_art::Update(game.Get(), 1000, false);
      lv_obj_update_layout(scene);
      SyncFacilityTargets(hits);
      lv_obj_update_layout(scene);
      for (unsigned id = 0; id < 9; ++id) {
        if (id == 4) { assert(!hits[id]); continue; }
        assert(lv_obj_has_flag(hits[id], LV_OBJ_FLAG_HIDDEN) == !built);
        if (!built) continue;
        HideModal(nullptr);
        lv_point_t p{lv_obj_get_x(hits[id]) + lv_obj_get_width(hits[id])/2,
                     lv_obj_get_y(hits[id]) + lv_obj_get_height(hits[id])/2};
        auto* found = lv_indev_search_obj(scene, &p);
        if (found != hits[id]) std::cerr << "blocked facility " << id << " stage " << stage << " at " << p.x << "," << p.y << "\n";
        assert(found == hits[id]);
        lv_obj_send_event(found, LV_EVENT_CLICKED, nullptr);
        assert(modal_kind == 15 && economy_page == (id == 8 ? 7 : id));
      }
    }
  }
  HideModal(nullptr);
  game.s.economy.built = 255;
  spring_art::Update(game.Get(), 1000, false);
  lv_obj_update_layout(scene);
  SyncFacilityTargets(hits);
  lv_obj_update_layout(scene);
  for (unsigned i = 0; i < 4; ++i) {
    lv_point_t p{int(i == 3 ? 446 : 130 + 104 * i), 420};
    assert(lv_indev_search_obj(scene, &p) == plots[i]);
  }
  lv_obj_delete(tree_hit);
  for (auto* plot : plots) lv_obj_delete(plot);
  for (auto* hit : hits) if (hit) lv_obj_delete(hit);
  game.s.tree_stage = 0;
  const auto water = game.s.water;
  StoryModal(23);
  StoryConfirm();
  assert(game.s.water == water - 2);
  StoryConfirm();
  assert(game.s.water == water - 2);
  StoryModal(22);
  StoryConfirm();
  assert(game.s.story.robot_named &&
         std::string(game.s.story.robot_name.data()) == "小芽");
  for (unsigned phase = 1; phase <= 4; ++phase) {
    game.s.story.flower = phase;
    spring_art::Update(game.Get(), 1000 + phase * 1000, false);
    HideModal(nullptr);
    lv_refr_now(display);
    capture(std::string(argv[1]) + "/flower-stage-" + std::to_string(phase) +
            ".ppm");
  }
  // New chapter UI uses the same modal and worker command: a tap is an explicit
  // invitation, not a fake completed rest or resource transaction.
  game = {};
  game.storage_ok = true;
  game.EnableEconomy(true);
  game.EnableMemory(57, 22);
  game.EnableStory();
  game.s.story_stage = 1;
  game.s.water = game.s.energy = 20;
  game.s.economy.paid_mask = 7;
  for (auto& p : game.s.plots) { p.ready = false; p.growth_ms = 200000; }
  game.s.story.completed = 1u << 4;
  game.s.story.available = (1u << 4) | (1u << 6);
  game.s.story.archives = 2;
  game.s.chapters.flags = spring::BeyondWorkObserved;
  story_selected = 6;
  StoryModal(24);
  RenderStoryModal(game.Get());
  assert(std::string(lv_label_get_text(lv_obj_get_child(primary, 0))) == "邀请坐下");
  lv_refr_now(display);
  capture(std::string(argv[1]) + "/chapter2-seat-invitation.ppm");
  const auto seat_water = game.s.water, seat_energy = game.s.energy;
  StoryConfirm();
  assert(spring::ChapterBench(game.s.chapters));
  assert(!spring::ChapterHas(game.s.chapters, spring::BenchRestCompleted));
  assert(game.s.life.under_tree == 2);
  assert(game.s.water == seat_water && game.s.energy == seat_energy);
  for (uint64_t now = 100; now <= 40000; now += 100) {
    game.Step(100);
    game.StepLife(100, false, spring_art::GardenerLifeArrived(game.Get()), false);
    spring_art::AdvanceMotion(game.Get(), now, false);
    if (game.s.life.phase == 3) {
      spring_art::Update(game.Get(), now, false);
      lv_refr_now(display);
      capture(std::string(argv[1]) + "/chapter2-seat-rest.ppm");
      break;
    }
  }
  assert(game.s.life.phase == 3 && game.s.life.reached);
  std::cout << pages << " actual story/archive pages; " << glyphs
            << " device-font glyph checks; flower/name confirmation and costs "
               "passed\n";
  lv_deinit();
}
