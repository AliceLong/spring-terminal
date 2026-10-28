#include "game/spring_agency.h"
#include "game/spring_social.h"
#include "game/spring_tool_json.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace spring;
void emit(const std::string &name, const std::string &text) {
  assert(GameToolReplyBytes(text) <= kGameToolReplyBudget);
  assert(text.find("reply_budget_exceeded") == std::string::npos);
  std::cout << "{\"case\":" << StoryQuote(name.c_str())
            << ",\"estimate\":" << GameToolReplyBytes(text)
            << ",\"text\":" << StoryQuote(text.c_str()) << "}\n";
}
int main() {
  Core core;
  core.storage_ok = true;
  core.EnableEconomy(true);
  core.EnableMemory(1, 42);
  core.EnableStory();
  core.Wake();
  const auto max = std::numeric_limits<uint32_t>::max();
  auto s = core.Get();
  s.plant_action = {max,
                    2,
                    3,
                    max,
                    PlantActionStatus::Running,
                    PlantActionReason::Unavailable,
                    max,
                    max,
                    max,
                    max,
                    true};
  s.water = s.energy = s.fiber = s.revision = max;
  s.game_ms = std::numeric_limits<uint64_t>::max();
  s.offline_seconds = max;
  s.economy.built = 255;
  s.economy.decision_count = 12;
  s.economy.food = s.economy.parts = s.economy.scrap = max;
  s.economy.proposal.token = max;
  s.economy.proposal.kind = 1;
  s.economy.proposal.target = 7;
  s.life.count = 6;
  for (auto &e : s.life.records) {
    e.id = e.second = max;
    e.intent = 2;
    e.target = 3;
    e.plant = 3;
    e.reason = 1;
    e.result = 1;
  }
  auto &n = s.story;
  n.archives = 127;
  n.available = (1u << kStoryCount) - 1;
  n.count = 16;
  n.robot_named = true;
  n.candidate_who = 2;
  for (auto *name : {&n.robot_name, &n.player_name, &n.candidate}) {
    name->fill('"');
    name->back() = 0;
  }
  for (unsigned i = 0; i < n.records.size(); ++i) {
    n.records[i].node = i;
    n.records[i].action = 6;
    n.records[i].second = n.records[i].value = max;
  }
  s.memory = std::make_shared<MemoryData>();
  auto &m = *s.memory;
  m.count = 128;
  m.fact_count = 32;
  m.last_id = 128;
  m.dropped = m.generation = max;
  for (unsigned i = 0; i < m.records.size(); ++i) {
    auto &e = m.records[i];
    e.id = i + 1;
    e.chain = e.second = e.cost = e.food_cost = e.value = max;
    e.category = 2;
    e.action = 8;
    e.actor = 4;
    e.kind = 7;
    e.flags = 1;
  }
  for (unsigned i = 0; i < m.facts.size(); ++i) {
    auto &f = m.facts[i];
    f.type = i + 1;
    f.event = i + 1;
    f.chain = f.second = f.value = f.cost = f.food_cost = max;
    f.actor = 4;
    f.kind = 7;
    f.flags = 1;
  }
  const auto original_story = n;
  auto social = EmptySocial(s);
  social.last_offered_id = max;
  social.accepted = {max, 3, 0, 0, 3, 0, max, 0, 1};
  const auto communication =
      std::string(
          "{\"source\":\"LIFE\",\"session_status\":\"LISTENING\",\"social\":") +
      SocialJson(s, social, true) + "}";
  auto character = EmptyCharacter(s);
  character.focus_kind = 2;
  character.focus_source = character.focus_chain = max;
  character.focus_target = 3;
  character.important_invitation = {max, 2, 3, 3, 4, 0, max, 1, 1};
  character.invitation_chain = max;
  character.focus_life = character.important_invitation;
  auto expanded_communication = communication;
  expanded_communication.pop_back();
  expanded_communication +=
      ",\"character\":" + CharacterJson(s, character) + "}";
  emit("shared-character-world", ToolWorldJson(s, 99, expanded_communication));
  auto agency = EmptyAgency(s);
  agency.next_token = INT32_MAX;
  agency.current = {INT32_MAX, 3, 0, max, max, max, max, 0, 1, 4};
  agency.count = 6;
  agency.history.fill(agency.current);
  auto with_agency = communication;
  with_agency.pop_back();
  with_agency += ",\"character\":" + CharacterSummaryJson(s, character) + "}";
  with_agency.pop_back();
  with_agency += " ,\"agency\":" + AgencySummaryJson(s, agency, true) + "}";
  emit("agency-expanded-world", ToolWorldJson(s, 99, with_agency));
  for (unsigned call = 0; call < 8; ++call) {
    auto json = ToolWorldJson(s, call == 7 ? 99 : call, with_agency);
    json.pop_back();
    json += ",\"incoming_communication\":{\"id\":5,\"opening\":" +
            StoryQuote(Opening(5, s)) + ",\"unread\":true}}";
    emit("agency-with-notice-" + std::to_string(call), json);
  }
  s.story.archives_read = 127;
  emit("character-agency-all-known-archives",
       AgencyObservationJson(s, character, agency));
  emit("character-all-known-archives", ObservationJson(s, character));
  for (unsigned archive = 0; archive < 7; ++archive) {
    auto typed = social;
    typed.accepted_kind = 2;
    typed.accepted_source = archive;
    const auto message = std::string("{\"source\":\"LIFE\",\"session_status\":"
                                     "\"LISTENING\",\"social\":") +
                         SocialJson(s, typed, true) +
                         ",\"character\":" + CharacterJson(s, character) + "}";
    emit("known-archive-world-" + std::to_string(archive),
         ToolWorldJson(s, 99, message));
  }

  for (unsigned region = 0; region < 5; ++region) {
    s.economy.region = region;
    for (unsigned call = 0; call < 8; ++call)
      emit("world-" + std::to_string(region) + "-" + std::to_string(call),
           ToolWorldJson(s, call == 7 ? 99 : call, communication));
  }
  s.economy.journey_task = 1;
  s.economy.journey_work = 120000;
  s.energy = 0;
  const auto active_journey = ToolWorldJson(s);
  assert(active_journey.find("\"name\":\"检查废弃设备\"") != std::string::npos);
  assert(active_journey.find("\"paused\":true") != std::string::npos);
  assert(active_journey.find("\"remaining_seconds\":120") != std::string::npos);
  emit("active-journey", active_journey);
  s.economy.journey_task = s.economy.journey_work = 0;
  emit("life", LifeJson(s));
  emit("events", EventsJson(s));
  for (unsigned node = 0; node < kStoryCount; ++node)
    for (unsigned offset = 0; offset <= 16; ++offset)
      emit("story-" + std::to_string(node) + "-" + std::to_string(offset),
           ToolStoryJson(s, node, offset == 16 ? 0 : offset, 6,
                         offset && offset < 16 ? StoryRecordCursor(s, offset)
                                               : ""));
  for (unsigned before = 0; before <= 129; ++before)
    emit("memory-records-" + std::to_string(before),
         ToolMemoryJson(s, 0, 0, 0, 24, before, 0, 8));
  for (unsigned offset = 0; offset <= 32; ++offset)
    emit("memory-facts-" + std::to_string(offset),
         ToolMemoryJson(s, 0, 0, 0, 1, 1, offset, 8));
  assert(n.count == original_story.count && m.count == 128 &&
         m.fact_count == 32);
  // A USB monitor retains full history and does not use the cloud budget
  // profile.
  auto full = WorldJson(s);
  assert(full.find("\"plan_history\"") != std::string::npos);
  assert(full.find("\"rules\"") != std::string::npos);
}
