#include "game/spring_agency.h"
#include "game/spring_social.h"
#include "spring_roadshow.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>

// Exercise the board's actual transaction code with fault-injected NVS handles.
using nvs_handle_t = unsigned;
constexpr int ESP_OK = 0;
std::map<std::pair<unsigned, std::string>, std::vector<unsigned char>> blobs;
unsigned failed_handle = 0;
int nvs_open_from_partition(const char *, const char *ns, int,
                            unsigned *handle) {
  assert(std::string(ns) == "roadshow_v1");
  *handle = 2;
  return 0;
}
constexpr int NVS_READWRITE = 1;
int nvs_set_blob(unsigned handle, const char *key, const void *data,
                 size_t size) {
  if (handle == failed_handle)
    return -1;
  const auto *bytes = static_cast<const unsigned char *>(data);
  blobs[{handle, key}] = {bytes, bytes + size};
  return 0;
}
int nvs_commit(unsigned handle) { return handle == failed_handle ? -1 : 0; }
void nvs_close(unsigned) {}
int nvs_get_blob(unsigned handle, const char *key, void *data, size_t *size) {
  auto found = blobs.find({handle, key});
  if (found == blobs.end() || *size < found->second.size())
    return -1;
  *size = found->second.size();
  std::memcpy(data, found->second.data(), *size);
  return 0;
}
uint32_t esp_random() {
  static uint32_t id = 100;
  return ++id;
}
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
nvs_handle_t roadshow_handle = 0;
std::atomic<uint32_t> roadshow_applied_epoch{0};
std::atomic<bool> roadshow_pending{false}, utc_ready{false},
    wish_ui_request{false};
std::atomic<unsigned> mind_boot{42}, mind_offer{0}, mind_mask{0},
    communication_ui_request{0}, story_ui_request{0}, active_call{99},
    opening_request{99}, proposal_request{0}, wave_status{0};
std::atomic<uint64_t> mind_deadline{0}, mind_reply_deadline{0};
std::mutex plant_status_mutex;
std::array<spring::PlantAction, 4> plant_statuses{};
void PublishSocial(const spring::SocialState &) {}
void PublishCharacter(const spring::CharacterState &) {}
void PublishAgency(const spring::AgencyState &) {}
void PowerActivity(const char *) {}
bool Persist(unsigned handle, const spring::Core &c) {
  const auto save = c.PackStory();
  return save && nvs_set_blob(handle, "save", save.get(), sizeof(*save)) == 0;
}
bool PersistSocial(unsigned h, bool, const spring::SocialState &s) {
  return !nvs_set_blob(h, "social", &s, sizeof(s));
}
bool PersistCharacter(unsigned h, bool, const spring::CharacterState &s) {
  return !nvs_set_blob(h, "character", &s, sizeof(s));
}
bool PersistAgency(unsigned h, bool, const spring::AgencyState &s) {
  return !nvs_set_blob(h, "agency", &s, sizeof(s));
}
bool LoadGame(unsigned h, bool demo, spring::Core &c) {
  auto saved = std::make_unique<spring::StorySave>();
  size_t size = sizeof(*saved);
  c.storage_ok = true;
  c.demo = demo;
  return !nvs_get_blob(h, "save", saved.get(), &size) && c.RestoreStory(*saved);
}
spring::SocialState LoadSocial(unsigned h, const spring::Snapshot &) {
  spring::SocialState s;
  size_t n = sizeof(s);
  assert(!nvs_get_blob(h, "social", &s, &n));
  return s;
}
spring::CharacterState LoadCharacter(unsigned h, const spring::Snapshot &) {
  spring::CharacterState s;
  size_t n = sizeof(s);
  assert(!nvs_get_blob(h, "character", &s, &n));
  return s;
}
spring::AgencyState LoadAgency(unsigned h, const spring::Snapshot &) {
  spring::AgencyState s;
  size_t n = sizeof(s);
  assert(!nvs_get_blob(h, "agency", &s, &n));
  return s;
}
#include "spring_roadshow.inc"

int main() {
  spring::Core formal;
  formal.storage_ok = true;
  formal.EnableEconomy(true);
  formal.EnableMemory(10, 20);
  formal.EnableStory();
  auto social = spring::EmptySocial(formal.Get());
  auto character = spring::EmptyCharacter(formal.Get());
  auto agency = spring::EmptyAgency(formal.Get());
  const auto original = formal.PackStory();
  nvs_handle_t handle = 1;
  unsigned request = 0;
  auto packet = [&](unsigned scene) {
    return spring_show::Packet{42,
                               ++request,
                               formal.roadshow_epoch,
                               formal.s.memory->world_hi,
                               formal.s.memory->world_lo,
                               scene};
  };
  auto apply = [&](unsigned scene) {
    SubmitRoadshow(packet(scene), formal, 100);
    return ApplyRoadshow(formal, handle, 1, social, character, agency, 200,
                         true, true);
  };
  assert(Persist(1, formal));
  const auto formal_blob = blobs.at({1, "save"});
  // Bad destination saves cannot switch the world or lose the formal return
  // point.
  failed_handle = 2;
  assert(!apply(1));
  assert(handle == 1 && !formal.roadshow && !frozen_formal);
  assert(formal.PackStory()->checksum == original->checksum);
  failed_handle = 0;
  for (unsigned scene : {1u, 2u, 3u, 4u, 5u, 6u, 8u, 9u, 10u, 11u}) {
    assert(apply(scene));
    assert(handle == 2 && formal.roadshow && formal.demo &&
           formal.roadshow_scene == scene);
    assert(blobs.at({1, "save"}) == formal_blob);
    assert(spring::AgencyLevel(formal.s.story) <= 1);
    assert(formal.s.memory->world_hi != 10 && formal.s.memory->world_lo != 20);
    auto saved = formal.PackStory();
    spring::Core restored;
    restored.storage_ok = true;
    restored.demo = true;
    assert(restored.RestoreStory(*saved));
    assert(restored.s.story.language == formal.s.story.language);
    // The persisted destination must be the new scene, not overwritten by the
    // source.
    spring::Core disk;
    assert(LoadGame(2, true, disk));
    assert(disk.s.memory->world_hi == formal.s.memory->world_hi);
    for (unsigned i = 0; i < formal.s.memory->count; ++i)
      assert(formal.s.memory->records[i].actor !=
             unsigned(spring::MemoryActor::Player));
    const auto before = formal.s.game_ms;
    formal.Step(100);
    assert(formal.s.game_ms == before + 100);
  }
  // Stale packets from another scene/boot never alter the current world.
  auto stale = packet(2);
  stale.epoch--;
  SubmitRoadshow(stale, formal, 100);
  assert(!roadshow_pending.load());
  auto wrongboot = packet(2);
  wrongboot.boot = 43;
  SubmitRoadshow(wrongboot, formal, 100);
  assert(!roadshow_pending.load());
  // A live voice or task waits; an expired request does not execute later.
  auto waiting = packet(1);
  SubmitRoadshow(waiting, formal, 100);
  assert(!ApplyRoadshow(formal, handle, 1, social, character, agency, 200,
                        false, true));
  assert(roadshow_pending.load());
  assert(!ApplyRoadshow(formal, handle, 1, social, character, agency, 60100,
                        true, true));
  assert(!roadshow_pending.load());
  // Returning preserves all formal story, resources and memories exactly.
  assert(apply(0));
  assert(handle == 1 && !formal.roadshow && !frozen_formal);
  assert(formal.PackStory()->checksum == original->checksum);
  // Continue restores the independent roadshow save, never cloning the formal
  // world.
  assert(apply(7));
  assert(formal.roadshow_scene == 11 && handle == 2);
  // Reboot recovery uses the bound formal identity and suppresses roadshow-time
  // settlement.
  spring::Core booted;
  assert(LoadGame(1, false, booted));
  assert(!booted.clock_ok);
  RecoverFormalRoadshow(booted);
  assert(booted.clock_ok && !booted.roadshow);
  assert(booted.PackStory()->checksum == original->checksum);
  assert(apply(0));
  assert(formal.PackStory()->checksum == original->checksum);
  // Duplicate delivery after an applied transition cannot replay it.
  const auto last = roadshow_receipt.packet;
  const auto epoch = formal.roadshow_epoch;
  SubmitRoadshow(last, formal, 500);
  assert(!roadshow_pending.load() && formal.roadshow_epoch == epoch);
  spring_show::Packet parsed;
  assert(spring_show::Line::Parse("SHOW1 2a 1 0 a 14 6", parsed));
  for (const char *bad : {"SHOW1 2a 1 0 a 14 c", "SHOW1 2a 1 0 a 14 1 extra",
                          "SHOW1 0 1 0 a 14 1", "SHOW1 2a 1 0 a 14 100000000",
                          "SHOW1 2a 1 0 a 14 -1"})
    assert(!spring_show::Line::Parse(bad, parsed));
  spring_show::Line parser;
  for (unsigned i = 0; i < 150; ++i)
    assert(!parser.Feed('x', parsed));
  assert(!parser.Feed('\n', parsed));
  bool received = false;
  for (char c : std::string("SHOW1 2a 1 0 a 14 1\n"))
    received |= parser.Feed(c, parsed);
  assert(received);
  std::puts(
      "Roadshow: ten fixtures, real save roundtrips, namespace isolation, "
      "source preservation, return/resume, failed storage, stale/duplicate "
      "requests, safe waiting, timeout and parser bounds passed");
}
