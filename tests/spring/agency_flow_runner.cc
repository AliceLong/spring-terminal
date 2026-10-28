// Host-only integration endpoint. No ESP transport, NVS, or production save
// access.
#include "game/spring_agency.h"
#include "game/spring_mind_slot.h"
#include "game/spring_monitor_json.h"
#include "game/spring_social.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace spring;

Core IsolatedWorld() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(0x5151, 0x6161);
  c.Wake();
  c.s.story.enabled = 1;
  c.s.story.active = kNoStory;
  c.s.story.language = 1;
  c.s.water = c.s.energy = 40;
  for (auto &p : c.s.plots) {
    p.ready = false;
    p.growth_ms = p.mature_ms = 0;
  }
  // A real Core inspection outcome gives the model a concrete recent
  // experience.
  assert(c.InviteLife(2, 0));
  c.StepLife(100, false, false, false);
  c.StepLife(100, false, true, false);
  for (unsigned i = 0; i < 21; ++i)
    c.StepLife(1000, false, true, false);
  assert(!c.LifeActive());
  c.s.life.cooldown = 0;
  return c;
}

uint32_t Mask(const Core &c) {
  uint32_t mask = 0;
  for (unsigned intent = 1; intent <= 3; ++intent)
    for (unsigned target = 0; target < (intent == 2 ? c.PlotCount() : 1u);
         ++target)
      if (AgencyMode(c.s.story, intent, target) &&
          c.CanConsiderLocalMindLife(intent, target))
        mask |= 1u << ((intent - 1) * 4 + target);
  return mask;
}

std::string Fixture(const Core &c, const AgencyState &a) {
  MonitorRuntime r;
  r.uptime_ms = 5000;
  r.voice = "idle";
  r.fsm = "idle";
  r.power = "游玩";
  r.mind_boot = r.mind_offer = 1;
  r.mind_mask = Mask(c);
  r.mind_sequence = c.s.life.sequence;
  r.mind_deadline = r.mind_reply_deadline = 25000;
  auto character = EmptyCharacter(c.Get());
  auto obs = ObservationJson(c.Get(), character);
  obs.pop_back();
  r.observation_json = obs + ",\"agency\":" + AgencyJson(c.Get(), a) + "}";
  r.communication_json =
      "{\"agency\":" + AgencyJson(c.Get(), a) +
      ",\"social\":" + SocialJson(c.Get(), EmptySocial(c.Get()), false) + "}";
  return MonitorJson(c.Get(), r, 99);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  auto c = IsolatedWorld();
  auto a = EmptyAgency(c.Get());
  if (scenario == "fixture") {
    std::cout << Fixture(c, a) << '\n';
    return 0;
  }
  std::string line;
  std::getline(std::cin, line);
  MindLine parser;
  MindPacket packet{};
  bool parsed = false;
  for (char ch : line + "\n")
    if (parser.Feed(ch, packet))
      parsed = true;
  assert(parsed);
  const MindReplyGuard guard{1, 1, 0x5151, 0x6161, c.s.life.sequence, 25000};
  assert(guard.BodyAvailable(packet, 6000, Mask(c)));
  assert(PersonalLife(packet.intent, packet.target));
  const auto initial = c.Get();
  auto character = EmptyCharacter(c.Get());
  bool comma = false;
  std::cout << "{\"scenario\":" << StoryQuote(scenario.c_str())
            << ",\"simulated_world\":true,\"hardware\":false,\"steps\":[";
  auto trace = [&](const char *label) {
    if (comma)
      std::cout << ',';
    comma = true;
    std::cout << "{\"step\":" << StoryQuote(label)
              << ",\"life_active\":" << (c.LifeActive() ? "true" : "false")
              << ",\"life_sequence\":" << c.s.life.sequence << ",\"query\":"
              << AgencyObservationJson(c.Get(), character, a, true) << '}';
  };
  assert(OfferWish(c.Get(), a, packet.intent, packet.target));
  assert(!c.LifeActive());
  trace("proposed_without_movement");
  if (scenario == "restart_pending") {
    assert(RecoverPendingWish(a));
    assert(a.current.status == unsigned(WishStatus::RestartCancelled));
    trace("restart_cancelled_without_movement");
  } else {
    assert(PresentWish(a, a.current.token));
    assert(!c.LifeActive());
    trace("opened_without_approval");
    const auto token = a.current.token;
    WishReplyContext voice{true, true, 9, 8};
    auto choice = 1u;
    auto offered_token = token;
    if (scenario == "same_turn")
      voice.current_turn = 8;
    else if (scenario == "wrong_session")
      voice.in_wish_session = false;
    else if (scenario == "wrong_token")
      ++offered_token;
    else if (scenario == "defer")
      choice = 2;
    else if (scenario == "decline")
      choice = 3;
    else if (scenario == "work_before_approval")
      c.work = Work::Water;
    else if (scenario == "stage_regressed")
      c.s.story.language = 0;
    else if (scenario == "story_busy")
      c.s.story.active = 1;
    const auto decision =
        DecideWishReply(c.Get(), a, offered_token, choice, voice);
    if (decision == WishReplyDecision::Rejected) {
      assert(scenario == "same_turn" || scenario == "wrong_session" ||
             scenario == "wrong_token" || scenario == "stage_regressed" ||
             scenario == "story_busy");
      assert(!c.LifeActive());
      trace("unauthorized_reply_rejected");
    } else if (decision == WishReplyDecision::Defer ||
               decision == WishReplyDecision::Decline) {
      FinishWish(a, decision == WishReplyDecision::Defer
                        ? WishStatus::Deferred
                        : WishStatus::Declined);
      assert(!c.LifeActive());
      trace("choice_recorded_without_movement");
    } else {
      assert(decision == WishReplyDecision::Approve);
      if (scenario == "work_before_approval") {
        const auto saved = c.PackFull();
        assert(!StageWishApproval(c, a, token));
        assert(c.PackFull() == saved && !c.LifeActive());
        trace("work_conflict_without_false_approval");
      } else {
        assert(StageWishApproval(c, a, token));
        const auto sequence = c.s.life.sequence;
        trace("approved_actual_request_created");
        assert(DecideWishReply(c.Get(), a, token, 1, voice) ==
               WishReplyDecision::AlreadyExecuting);
        assert(!StageWishApproval(c, a, token));
        assert(sequence == c.s.life.sequence);
        trace("retry_without_second_action");
        if (scenario == "interrupt_before_arrival") {
          c.work = Work::Water;
          c.StepLife(100, true, false, false);
        } else if (scenario == "restart_executing") {
          c.FinishLife(LifeResult::Rebooted);
        } else {
          c.StepLife(100, true, false, false);
          trace("traveling_during_conversation");
          // The host supplies arrival, just as the hardware renderer does.
          c.StepLife(100, true, true, false);
          RefreshAgency(c.Get(), a);
          assert(a.current.reached);
          trace("arrived_during_conversation");
          if (scenario == "interrupt_after_arrival") {
            c.work = Work::Water;
            c.StepLife(100, true, true, false);
          } else {
            assert(scenario == "approve");
            for (unsigned i = 0; i < 65; ++i)
              c.StepLife(1000, true, true, false);
          }
        }
        assert(!c.LifeActive() && RefreshAgency(c.Get(), a));
        const bool completed = scenario == "approve";
        assert(a.current.status == unsigned(completed
                                                ? WishStatus::Completed
                                                : WishStatus::Interrupted));
        assert(a.count == 1 && a.current.sequence == sequence &&
               a.current.chain);
        if (completed)
          assert(a.current.reached);
        trace(completed ? "completed_with_queryable_evidence"
                        : "interrupted_not_reported_as_completed");
      }
    }
  }
  assert(c.s.water == initial.water && c.s.energy == initial.energy &&
         c.s.fiber == initial.fiber &&
         c.s.economy.food == initial.economy.food &&
         c.s.economy.parts == initial.economy.parts);
  std::cout << "],\"resources_unchanged\":true,\"assertions_pass\":true}\n";
}
