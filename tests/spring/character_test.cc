#include "game/spring_character.h"
#include "game/spring_mind_slot.h"
#include "game/spring_monitor_json.h"
#include "game/spring_social.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace spring;
Core Fresh() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(11, 22);
  c.Wake();
  c.s.water = c.s.energy = 40;
  for (auto &p : c.s.plots) {
    p.ready = false;
    p.growth_ms = 0;
    p.mature_ms = 0;
  }
  c.s.life.cooldown = 0;
  c.s.story.enabled = 1;
  return c;
}
void Inspect(Core &c) {
  assert(c.InviteLife(2, 1));
  c.StepLife(100, false, false, false);
  c.StepLife(100, false, true, false);
  for (int i = 0; i < 21; ++i)
    c.StepLife(1000, false, true, false);
  assert(!c.LifeActive());
  c.s.life.cooldown = 0;
  c.s.life.repeat_cooldown = {};
}
void Fixture(std::ostream &out, const char *name, Core c,
             CharacterState character, SocialState social, bool comma) {
  MonitorRuntime r;
  r.uptime_ms = 5000;
  r.voice = "idle";
  r.power = "游玩";
  r.fsm = "idle";
  r.mind_boot = 1;
  r.mind_offer = 1;
  r.mind_reply_deadline = 25000;
  for (unsigned i = 1; i <= 3; ++i)
    for (unsigned t = 0; t < (i == 2 ? c.PlotCount() : 1u); ++t)
      if (c.CanRequestLocalMindLife(i, t))
        r.mind_mask |= 1u << ((i - 1) * 4 + t);
  r.mind_deadline = r.mind_mask ? 25000 : 0;
  r.mind_sequence = c.s.life.sequence;
  const auto e = NextAttention(c.Get(), character);
  r.mind_trigger_kind = e.kind;
  r.mind_trigger_source = e.source;
  r.mind_trigger_target = e.target;
  r.observation_json = ObservationJson(c.Get(), character);
  r.communication_json = "{\"social\":" + SocialJson(c.Get(), social, false) +
                         ",\"character\":" + CharacterJson(c.Get(), character) +
                         "}";
  auto rule = c;
  rule.StepLife(100, false, false, true);
  if (comma)
    out << ',';
  out << "{\"name\":" << StoryQuote(name)
      << ",\"simulated\":true,\"snapshot\":" << MonitorJson(c.Get(), r, 99)
      << ",\"rule\":{\"intent\":"
      << StoryQuote(LifeIntentName(rule.s.life.intent))
      << ",\"target\":" << rule.s.life.target << "}}";
}
int main(int argc, char **argv) {
  auto c = Fresh();
  auto character = EmptyCharacter(c.Get());
  auto social = EmptySocial(c.Get());
  assert(!NextAttention(c.Get(), character).kind);
  Inspect(c);
  assert(RefreshCharacter(c.Get(), character));
  const auto id = character.important_invitation.id;
  assert(id && character.invitation_chain &&
         NextAttention(c.Get(), character).kind == 1);
  assert(SetCharacterFocus(c.Get(), character, 2, 2, id, 1));
  assert(!SetCharacterFocus(c.Get(), character, 2, 2, id, 0));
  assert(OfferSocialEvidence(c.Get(), 1, id, social, &character));
  assert(social.pending_source == id);
  assert(SocialJson(c.Get(), social, false).find("\"target_slot\":2") !=
         std::string::npos);
  assert(CharacterJson(c.Get(), character).find("下次检查这株植物。") !=
         std::string::npos);
  assert(SocialJson(c.Get(), social, false).find("\"player_requested\":true") !=
         std::string::npos);
  auto saved = character;
  auto checkpoint = c.PackFull();
  Core reboot;
  reboot.storage_ok = true;
  assert(reboot.RestoreFull(checkpoint));
  const auto memory_checkpoint = c.PackArchive();
  assert(memory_checkpoint && reboot.RestoreArchive(*memory_checkpoint));
  assert(CharacterMatches(reboot.Get(), saved));
  assert(saved.focus_kind == 2 && saved.important_invitation.id == id);
  reboot.s.life.count = 0;
  reboot.WriteMemory().count = 0;
  auto after_eviction = EmptySocial(reboot.Get());
  assert(OfferSocialEvidence(reboot.Get(), 1, id, after_eviction, &saved));
  assert(CharacterJson(reboot.Get(), saved).find("player_watched\":null") !=
         std::string::npos);
  auto animal = Fresh();
  animal.s.economy.animals = 1;
  auto animal_character = EmptyCharacter(animal.Get());
  assert(animal.InviteLife(2, 5));
  animal.StepLife(100, true, false, false);
  animal.StepLife(100, true, true, false);
  for (int i = 0; i < 21; ++i)
    animal.StepLife(1000, true, true, false);
  assert(RefreshCharacter(animal.Get(), animal_character));
  assert(SetCharacterFocus(animal.Get(), animal_character, 2, 2,
                           animal_character.important_invitation.id, 5));
  const auto animal_json = CharacterJson(animal.Get(), animal_character);
  assert(animal_json.find("\"target_kind\":\"animal_home\"") !=
         std::string::npos);
  assert(animal_json.find("\"plant_id\":null") != std::string::npos);
  assert(animal_json.find("下次查看动物小窝。") != std::string::npos);
  auto other = Fresh();
  other.s.memory->world_hi = 99;
  assert(!CharacterMatches(other.Get(), saved));
  assert(!OfferSocialEvidence(other.Get(), 1, id, after_eviction, &saved));
  auto routine = Fresh();
  routine.s.life.count = 1;
  routine.s.life.records[0] = {7, 2, 0, 0, 2, 0, 42, 0, 1};
  auto routine_social = EmptySocial(routine.Get());
  assert(!OfferSocial(routine.Get(), 7, routine_social));
  auto archive = Fresh();
  auto knowledge = EmptyCharacter(archive.Get());
  archive.s.story.archives = 9;
  archive.s.story.archives_read = 1;
  assert(NextAttention(archive.Get(), knowledge).kind == 2);
  assert(ObservationJson(archive.Get(), knowledge).find("G-17") ==
         std::string::npos);
  assert(!SetCharacterFocus(archive.Get(), knowledge, 2, 3, 3, 0));
  assert(SetCharacterFocus(archive.Get(), knowledge, 2, 3, 0, 0));
  auto archive_social = EmptySocial(archive.Get());
  assert(!OfferSocialEvidence(archive.Get(), 2, 3, archive_social));
  assert(OfferSocialEvidence(archive.Get(), 2, 0, archive_social));
  assert(archive_social.pending.id && archive_social.pending_source == 0);
  MindPacket p;
  assert(MindLine::Parse("MIND3 1 1 b 16 0 0 0 0 2 2 3 0 0", p));
  assert(p.extended && p.social_kind == 2 && p.social_record == 0 &&
         p.focus_kind == 3);
  assert(!MindLine::Parse("MIND3 1 1 b 16 0 0 0 0 4 2 3 0 0", p));
  LegacySocialState old;
  old.world_hi = 11;
  old.world_lo = 22;
  old.last_offered_id = 265;
  old.topic_mask = 10;
  old.pending = {265, 3, 0, 0, 3, 0, 42, 1, 1};
  auto migrated = MigrateSocial(archive.Get(), old);
  assert(migrated.pending.id == 265 && migrated.pending_source == 265 &&
         migrated.topic_mask == 10);
  assert(AcceptSocial(265, migrated));
  assert(!AcceptSocial(265, migrated));
  auto delayed_core = c;
  auto delayed_character = character;
  const auto original_event =
      NextAttention(delayed_core.Get(), delayed_character);
  Inspect(delayed_core);
  RefreshCharacter(delayed_core.Get(), delayed_character);
  MarkAttention(delayed_character, original_event);
  assert(NextAttention(delayed_core.Get(), delayed_character).source >
         original_event.source);
  auto event = NextAttention(c.Get(), character);
  MarkAttention(character, event);
  assert(!NextAttention(c.Get(), character).kind);
  assert(SetCharacterFocus(c.Get(), character, 1, 0, 0, 0) &&
         !character.focus_kind);
  auto building = Fresh();
  auto bc = EmptyCharacter(building.Get());
  building.s.fiber = 40;
  building.s.economy.parts = 10;
  assert(building.Propose(1, 0));
  assert(building.ConfirmProposal(building.s.economy.proposal.token));
  assert(!NextAttention(building.Get(), bc).kind);
  building.Advance(kFacilities[0].ms);
  assert(building.HasFacility(0));
  assert(NextAttention(building.Get(), bc).kind == 3);
  auto milestone_social = EmptySocial(building.Get());
  auto build_event = NextAttention(building.Get(), bc);
  assert(OfferSocialEvidence(building.Get(), 3, build_event.source,
                             milestone_social));
  assert(SetCharacterFocus(building.Get(), bc, 2, 4, build_event.source, 20));
  const auto baseline = EmptyCharacter(building.Get());
  assert(!NextAttention(building.Get(), baseline).kind);
  if (argc > 1) {
    std::ofstream out(argv[1]);
    out << '[';
    auto a = Fresh();
    auto ac = EmptyCharacter(a.Get());
    auto as = EmptySocial(a.Get());
    Fixture(out, "ordinary_idle", a, ac, as, false);
    Inspect(a);
    RefreshCharacter(a.Get(), ac);
    Fixture(out, "invitation_completed", a, ac, as, true);
    auto low = a;
    low.s.water = 0;
    Fixture(out, "shortage_after_invitation", low, ac, as, true);
    auto known = Fresh();
    auto kc = EmptyCharacter(known.Get());
    known.s.story.archives = 2;
    known.s.story.archives_read = 2;
    Fixture(out, "read_performance_archive", known, kc,
            EmptySocial(known.Get()), true);
    auto locked = Fresh();
    locked.s.story.archives = 9;
    locked.s.story.archives_read = 1;
    auto lc = EmptyCharacter(locked.Get());
    Fixture(out, "locked_future_archive", locked, lc, EmptySocial(locked.Get()),
            true);
    building.s.life.cooldown = 0;
    building.s.life.repeat_cooldown = {};
    Fixture(out, "facility_completed", building, bc,
            EmptySocial(building.Get()), true);
    out << ']';
  }
  std::cout << "character: invitation evidence, focus, restart/eviction, "
               "gates, migration and typed protocol pass\n";
}
