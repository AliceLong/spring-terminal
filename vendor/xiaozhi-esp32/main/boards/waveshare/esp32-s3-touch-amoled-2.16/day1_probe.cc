#include "day1_probe.h"
#include <driver/usb_serial_jtag.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <hal/usb_serial_jtag_ll.h>
#include <mbedtls/base64.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <atomic>
#include <cmath>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>
#include <vector>
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "display/display.h"
#include "display/lvgl_display/lvgl_font.h"
#include "game/spring_agency.h"
#include "game/spring_mind_slot.h"
#include "game/spring_monitor_json.h"
#include "game/spring_social.h"
#include "game/spring_story_authority.h"
#include "game/touch_buffer.h"
#include "spring_art.h"
#include "spring_collection_view.h"
#include "spring_external_art.h"
#include "spring_feature_gates.h"
#include "spring_font.h"
#include "spring_power.h"
#include "spring_roadshow.h"
#include "spring_scene_input.h"
#include "spring_scene_touch.h"
#include "spring_touch.h"
#include "spring_touch_input.h"

namespace desk_gap {
namespace {
constexpr char kTag[] = "SpringTerminal";
std::array<std::atomic<uint32_t>, 18> monitor_queries{};
std::mutex world_mutex;
spring::Core world;
std::atomic<bool> motion_ok{false}, utc_ready{false}, picked_up{false};
std::atomic<float> tilt_angle{0};
std::atomic<bool> tilt_stable{false};
std::atomic<int64_t> tilt_at{0};
std::atomic<uint64_t> voice_wait_at{0};
std::atomic<unsigned> feedback{0}, active_call{99}, opening_request{99};
std::atomic<uint32_t> mind_boot{0}, mind_offer{0}, mind_mask{0}, mind_status{0}, mind_sequence{0};
std::atomic<uint64_t> mind_deadline{0};
std::atomic<uint64_t> mind_reply_deadline{0};
std::atomic<uint32_t> mind_received{0}, mind_parsed{0};
std::atomic<uint32_t> wave_next_id{0}, wave_request_id{0}, wave_status{0};
enum class CommunicationSource : uint8_t { None, Player, Life, Story };
std::atomic<CommunicationSource> communication_source{CommunicationSource::None};
std::atomic<unsigned> communication_archive{255}, communication_story_node{255},
    story_response_node{255};
spring::StoryPlaybackReceipt story_playback_receipt;  // LVGL owner only.
std::atomic<bool> communication_closing{false}, communication_close_sent{false};
std::mutex social_mutex;
spring::SocialState social_public;
spring::CharacterState character_public;
spring::AgencyState agency_public;
std::atomic<bool> wish_ui_request{false};
uint32_t wish_presented_turn = UINT32_MAX;
spring_collection::View collection_view;
std::atomic<bool> collection_practice{false};
std::atomic<unsigned> collection_shakes{0};
std::mutex collection_metrics_mutex;
spring_collection::Metrics collection_metrics;
std::atomic<uint32_t> mind_focus_status{0}, mind_trigger_kind{0}, mind_social_status{0};
std::atomic<uint32_t> mind_trigger_source{0}, mind_trigger_target{0};
std::atomic<bool> social_ui_request{false};
std::atomic<unsigned> communication_ui_request{0};
bool mind_input_ready = false;  // Written/read only by the world worker.
nvs_handle_t roadshow_handle = 0;
std::atomic<bool> roadshow_pending{false};
std::atomic<uint32_t> roadshow_input_epoch{0}, roadshow_applied_epoch{0};
enum class Action : uint32_t {
    Wake,
    Shake,
    Select,
    Build,
    Accept,
    Mode,
    Announce,
    Mute,
    Propose,
    ConfirmProposal,
    DismissProposal,
    DemoJourney,
    InviteLife,
    Wave,
    SocialAccept,
    Story,
    ResumeWork,
    WishOpen,
    WishReply,
    PlantWork,
    CancelActivity
};
struct Command {
    Action action;
    unsigned value, argument = 0, extra = 0;
    spring::MemoryActor actor = spring::MemoryActor::Player;
    uint32_t token = 0;
    std::array<char, 49> text{};
    uint32_t world_hi = 0, world_lo = 0;
    bool demo = false;
    uint32_t context_epoch = 0;
};
// Four bounded status receipts, independent of the economic save format.
std::mutex plant_status_mutex;
std::array<spring::PlantAction, 4> plant_statuses{};
uint32_t plant_next_id = 0;
void PublishPlantWork(const spring::PlantAction& a) {
    if (!a.id)
        return;
    std::lock_guard<std::mutex> guard(plant_status_mutex);
    plant_statuses[a.id % plant_statuses.size()] = a;
}
QueueHandle_t commands = nullptr;
std::atomic<unsigned> story_ui_request{0};
std::atomic<uint32_t> user_turn{0};
uint32_t flower_offer_turn = 0, name_offer_turn = 0, economy_offer_turn = 0;
bool Send(Action a, unsigned v = 0, unsigned argument = 0, unsigned extra = 0,
          spring::MemoryActor actor = spring::MemoryActor::Player) {
    if (roadshow_input_epoch.load() != roadshow_applied_epoch.load())
        return false;
    if (a != Action::Announce) {
        PowerActivity("game_input");
        if (a != Action::Shake)
            spring_art::GardenerActivity(esp_timer_get_time() / 1000);
    }
    Command c{a, v, argument, extra, actor};
    c.context_epoch = roadshow_input_epoch.load();
    if (commands && xQueueSend(commands, &c, 0) == pdTRUE)
        return true;
    feedback.store(6);  // Queue failure is distinct from a rejected game action.
    return false;
}
const char* SaveKey(bool demo) { return demo ? "demo_v4" : "world_v4"; }
const char* SocialKey(bool demo) { return demo ? "social_demo_v2" : "social_world_v2"; }
bool PersistSocial(nvs_handle_t handle, bool demo, const spring::SocialState& state) {
    return nvs_set_blob(handle, SocialKey(demo), &state, sizeof(state)) == ESP_OK &&
           nvs_commit(handle) == ESP_OK;
}
void PublishSocial(const spring::SocialState& state) {
    std::lock_guard<std::mutex> guard(social_mutex);
    social_public = state;
}
spring::SocialState LoadSocial(nvs_handle_t handle, const spring::Snapshot& s) {
    spring::SocialState state;
    size_t size = sizeof(state);
    const auto result = nvs_get_blob(handle, SocialKey(s.demo), &state, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        spring::LegacySocialState legacy;
        size = sizeof(legacy);
        if (nvs_get_blob(handle, s.demo ? "social_demo_v1" : "social_world_v1", &legacy, &size) ==
                ESP_OK &&
            size == sizeof(legacy))
            state = spring::MigrateSocial(s, legacy);
        else
            state = spring::EmptySocial(s);
    } else if (result != ESP_OK || size != sizeof(state) || !spring::SocialMatches(s, state)) {
        state = spring::EmptySocial(s);
        // Preserve a damaged sidecar; do not replay invitations by overwriting it.
        state.last_offered_id = UINT32_MAX;
        ESP_LOGE(kTag, "SOCIAL_STORAGE_UNAVAILABLE");
    }
    PublishSocial(state);
    return state;
}
const char* CharacterKey(bool demo) { return demo ? "char_demo_v1" : "char_world_v1"; }
bool PersistCharacter(nvs_handle_t handle, bool demo, const spring::CharacterState& state) {
    return nvs_set_blob(handle, CharacterKey(demo), &state, sizeof(state)) == ESP_OK &&
           nvs_commit(handle) == ESP_OK;
}
void PublishCharacter(const spring::CharacterState& state) {
    std::lock_guard<std::mutex> guard(social_mutex);
    character_public = state;
}
spring::CharacterState LoadCharacter(nvs_handle_t handle, const spring::Snapshot& s) {
    spring::CharacterState state;
    size_t size = sizeof(state);
    if (nvs_get_blob(handle, CharacterKey(s.demo), &state, &size) != ESP_OK ||
        size != sizeof(state) || !spring::CharacterMatches(s, state)) {
        state = spring::EmptyCharacter(s);
        if (!PersistCharacter(handle, s.demo, state))
            ESP_LOGE(kTag, "CHARACTER_SAVE_FAILED");
    }
    PublishCharacter(state);
    return state;
}
const char* AgencyKey(bool demo) { return demo ? "agency_demo_v1" : "agency_world_v1"; }
bool PersistAgency(nvs_handle_t handle, bool demo, const spring::AgencyState& state) {
    return nvs_set_blob(handle, AgencyKey(demo), &state, sizeof(state)) == ESP_OK &&
           nvs_commit(handle) == ESP_OK;
}
void PublishAgency(const spring::AgencyState& state) {
    std::lock_guard<std::mutex> guard(social_mutex);
    agency_public = state;
}
spring::AgencyState LoadAgency(nvs_handle_t handle, const spring::Snapshot& s) {
    auto state = spring::EmptyAgency(s);
    size_t size = sizeof(state);
    const auto result = nvs_get_blob(handle, AgencyKey(s.demo), &state, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        state = spring::EmptyAgency(s);
    } else if (result != ESP_OK || size != sizeof(state) || !spring::AgencyMatches(s, state)) {
        state = spring::EmptyAgency(s);
        state.next_token = INT32_MAX;  // Preserve damaged data, disable new offers.
        ESP_LOGE(kTag, "AGENCY_STORAGE_UNAVAILABLE");
    } else if (spring::RecoverPendingWish(state)) {
        if (!PersistAgency(handle, s.demo, state))
            state.next_token = INT32_MAX;
    }
    PublishAgency(state);
    return state;
}
std::string AgencyContextJson(const spring::Snapshot& s, bool active = false,
                              bool include_history = true) {
    std::lock_guard<std::mutex> guard(social_mutex);
    return spring::AgencyJson(s, agency_public, active, include_history);
}
std::string CharacterJson(const spring::Snapshot& s, bool observation) {
    spring::CharacterState state;
    {
        std::lock_guard<std::mutex> guard(social_mutex);
        state = character_public;
    }
    if (!observation)
        return spring::CharacterJson(s, state);
    auto json = spring::ObservationJson(s, state);
    json.pop_back();
    return json + ",\"agency\":" + AgencyContextJson(s) + "}";
}
std::string CommunicationJson(const spring::Snapshot& s, bool compact = false) {
    spring::SocialState social;
    {
        std::lock_guard<std::mutex> guard(social_mutex);
        social = social_public;
    }
    const auto source = communication_source.load();
    const auto audio = Application::GetInstance().GetDeviceState();
    const unsigned archive = communication_archive.load();
    std::ostringstream o;
    o << "{\"source\":\""
      << (source == CommunicationSource::Player  ? "PLAYER"
          : source == CommunicationSource::Life  ? "LIFE"
          : source == CommunicationSource::Story ? "STORY"
                                                 : "NONE")
      << "\",\"session_status\":\""
      << (communication_closing.load()                                         ? "CLOSING"
          : audio == kDeviceStateSpeaking                                      ? "SPEAKING"
          : audio == kDeviceStateListening                                     ? "LISTENING"
          : audio == kDeviceStateConnecting                                    ? "OPENING"
          : audio != kDeviceStateIdle                                          ? "OPENING"
          : source != CommunicationSource::None                                ? "IDLE"
          : (social.pending.id || spring::StoryCommunicationNode(s) < spring::kStoryCount) ? "PENDING"
                                                                               : "IDLE")
      << "\",\"social\":"
      << spring::SocialJson(s, social,
                            source == CommunicationSource::Life && active_call.load() == 99);
    if (compact) {
        std::lock_guard<std::mutex> guard(social_mutex);
        o << ",\"character\":" << spring::CharacterSummaryJson(s, character_public);
    } else
        o << ",\"character\":" << CharacterJson(s, false);
    {
        std::lock_guard<std::mutex> guard(social_mutex);
        const bool active = source == CommunicationSource::Life && active_call.load() == 98;
        o << ",\"agency\":"
          << (compact ? spring::AgencySummaryJson(s, agency_public, active)
                      : spring::AgencyJson(s, agency_public, active));
    }
    o << ",\"story_unread_mask\":" << (s.story.archives & ~s.story.archives_read)
      << ",\"pending_story_node\":" << s.story.pending;
    if (source == CommunicationSource::Story && archive < 7 && (s.story.archives & (1u << archive)))
        o << ",\"archive_id\":" << archive << ",\"event_id\":\"archive-" << archive
          << "\",\"topic\":" << spring::StoryQuote(spring::kStoryArchiveTitles[archive]);
    o << "}";
    return o.str();
}
std::atomic<unsigned> proposal_request{0};
// No migration from desk_gap_v1: the old game's namespace is never opened here.
bool Load(nvs_handle_t handle, bool demo, spring::Core& out) {
    out = {};
    out.demo = demo;
    // Startup/mode-switch only: keep migration buffers off the game task stack.
    std::unique_ptr<spring::FullSave> full(new (std::nothrow) spring::FullSave{});
    if (!full)
        return false;
    size_t length = sizeof(*full);
    auto result = nvs_get_blob(handle, SaveKey(demo), full->data(), &length);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        length = sizeof(*full);
        result = nvs_get_blob(handle, demo ? "demo_v3" : "world_v3", full->data(), &length);
    }
    if (result == ESP_OK) {
        if (length != sizeof(*full) || !out.RestoreFull(*full))
            return false;
    } else if (result == ESP_ERR_NVS_NOT_FOUND) {
        full.reset();
        std::unique_ptr<spring::LegacyFullSave> v2(new (std::nothrow) spring::LegacyFullSave{});
        if (!v2)
            return false;
        length = sizeof(*v2);
        result = nvs_get_blob(handle, demo ? "demo_v2" : "world_v2", v2->data(), &length);
        if (result == ESP_OK) {
            if (length != sizeof(*v2) || !out.RestoreFull(*v2))
                return false;
            out.demo = demo;
            out.storage_ok = true;
            return true;
        }
        if (result != ESP_ERR_NVS_NOT_FOUND)
            return false;
        spring::Save legacy{};
        length = sizeof(legacy);
        result = nvs_get_blob(handle, demo ? "demo_v1" : "world_v1", legacy.data(), &length);
        if (result == ESP_ERR_NVS_NOT_FOUND)
            out.EnableEconomy(true);
        else {
            if (result != ESP_OK || length != sizeof(legacy) || !out.Restore(legacy))
                return false;
            out.EnableEconomy(false);
        }
    } else
        return false;
    out.demo = demo;
    out.storage_ok = true;
    return true;
}
// New game writes own partition; the crowded shared NVS remains read-only for migration.
[[maybe_unused]] bool LoadGameLegacy(nvs_handle_t handle, bool demo, spring::Core& out) {
    size_t length = 0;
    const auto existing = nvs_get_blob(handle, SaveKey(demo), nullptr, &length);
    if (existing != ESP_ERR_NVS_NOT_FOUND)
        return existing == ESP_OK && Load(handle, demo, out);
    const auto v3 = nvs_get_blob(handle, demo ? "demo_v3" : "world_v3", nullptr, &length);
    if (v3 != ESP_ERR_NVS_NOT_FOUND)
        return v3 == ESP_OK && Load(handle, demo, out);
    // Prefer the old economy save in the game partition before shared-NVS fallback.
    const auto old = nvs_get_blob(handle, demo ? "demo_v2" : "world_v2", nullptr, &length);
    if (old != ESP_ERR_NVS_NOT_FOUND)
        return old == ESP_OK && Load(handle, demo, out);
    nvs_handle_t legacy = 0;
    const auto opened = nvs_open("spring_v1", NVS_READONLY, &legacy);
    if (opened == ESP_ERR_NVS_NOT_FOUND)
        return Load(handle, demo, out);
    if (opened != ESP_OK)
        return false;
    const bool loaded = Load(legacy, demo, out);
    nvs_close(legacy);
    ESP_LOGI(kTag, "GAME_STORAGE migration=%s source=shared_nvs destination=game_nvs mode=%s",
             loaded ? "loaded" : "failed", demo ? "demo" : "normal");
    return loaded;
}
const char* MemoryKey(bool demo) { return demo ? "mem_demo_v1" : "mem_world_v1"; }
const char* CheckpointKey(bool demo) { return demo ? "chk_demo_v1" : "chk_world_v1"; }
std::array<uint32_t, 3> memory_saved_generation{}, memory_saved_hi{}, memory_saved_lo{};
bool LoadGame(nvs_handle_t handle, bool demo, spring::Core& out) {
    out = {};
    out.demo = demo;
    out.storage_ok = true;
    auto save = std::make_unique<spring::StorySave>();
    size_t size = sizeof(*save);
    const auto result = nvs_get_blob(handle, demo ? "demo_v46" : "world_v46", save.get(), &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        if (!demo) {
            // V4.6 explicitly starts a new game. Reclaim only this game's retired namespace
            // in its dedicated partition; never open shared NVS, Wi-Fi, binding or desk_gap.
            nvs_handle_t retired = 0;
            const auto opened =
                nvs_open_from_partition("game_nvs", "spring_v1", NVS_READWRITE, &retired);
            if (opened != ESP_OK)
                return false;
            auto cleared = nvs_erase_all(retired);
            if (cleared == ESP_OK)
                cleared = nvs_commit(retired);
            nvs_close(retired);
            if (cleared != ESP_OK)
                return false;
        }
        out.EnableEconomy(true);
        out.EnableMemory(esp_random(), esp_random());
        out.EnableStory();
        return true;
    }
    if (result != ESP_OK || size != sizeof(*save) || !out.RestoreStory(*save))
        return false;
    auto checkpoint = std::make_unique<spring::StoryCheckpoint>();
    size_t checkpoint_size = sizeof(*checkpoint);
    if (nvs_get_blob(handle, demo ? "cdemo_v46" : "cworld_v46", checkpoint.get(),
                     &checkpoint_size) == ESP_OK &&
        checkpoint_size == sizeof(*checkpoint))
        out.RestoreStoryCheckpoint(*checkpoint);
    checkpoint.reset();
    if (out.s.story.active < spring::kStoryCount) {
        out.s.story.paused = out.s.story.active;
        out.s.story.active = spring::kNoStory;
        out.s.story.pending = spring::kNoStory;
    }
    return true;
}
std::array<uint32_t, 3> story_saved_revision{};
bool Persist(nvs_handle_t handle, const spring::Core& game) {
    if (!handle || !game.storage_ok || !game.s.memory)
        return false;
    const unsigned mode = handle == roadshow_handle ? 2 : game.demo ? 1 : 0;
    const auto& m = *game.s.memory;
    bool archived = false;
    if (memory_saved_generation[mode] != m.generation || memory_saved_hi[mode] != m.world_hi ||
        memory_saved_lo[mode] != m.world_lo ||
        story_saved_revision[mode] != game.s.story.revision) {
        auto save = game.PackStory();
        if (!save)
            return false;
        auto result =
            nvs_set_blob(handle, game.demo ? "demo_v46" : "world_v46", save.get(), sizeof(*save));
        if (result == ESP_OK)
            result = nvs_commit(handle);
        if (result != ESP_OK) {
            ESP_LOGE(kTag, "STORY_SAVE_FAILED %s", esp_err_to_name(result));
            return false;
        }
        memory_saved_generation[mode] = m.generation;
        memory_saved_hi[mode] = m.world_hi;
        memory_saved_lo[mode] = m.world_lo;
        story_saved_revision[mode] = game.s.story.revision;
        archived = true;
    }
    auto checkpoint = game.PackStoryCheckpoint();
    if (!checkpoint)
        return false;
    auto result = nvs_set_blob(handle, game.demo ? "cdemo_v46" : "cworld_v46", checkpoint.get(),
                               sizeof(*checkpoint));
    if (result == ESP_OK)
        result = nvs_commit(handle);
    return result == ESP_OK || archived;
}
#include "spring_roadshow.inc"
uint32_t MindMask(const spring::Core& game, bool voice_idle, bool allow_wish) {
    if (roadshow_pending.load() || !voice_idle || GamePowerMode() == spring_power::Mode::Sleep ||
        !game.s.memory || (game.s.story.enabled && game.s.story.active < spring::kStoryCount))
        return 0;
    uint32_t mask = 0;
    for (unsigned intent = 1; intent <= 3; ++intent)
        for (unsigned target = 0; target < (intent == 2 ? game.PlotCount() : 1u); ++target)
            if ((spring::AgencyMode(game.s.story, intent, target) == 2 ||
                 (allow_wish && spring::AgencyMode(game.s.story, intent, target) == 1)) &&
                game.CanConsiderLocalMindLife(intent, target, 1000))
                mask |= 1u << ((intent - 1) * 4 + target);
    return mask;
}
bool PollMind(spring::Core& game, uint64_t now, bool voice_idle, nvs_handle_t handle,
              spring::SocialState& social, spring::CharacterState& character,
              spring::AgencyState& agency, uint64_t& next_wish, uint64_t& wish_expires) {
    if (!mind_input_ready)
        return false;
    static spring::MindLine parser;
    static spring_show::Line show_parser;
    static uint64_t next_offer = 0;
    static spring::MindPacket pending;
    static bool has_pending = false;
    static bool offer_demo = false;
    static uint32_t offer_hi = 0, offer_lo = 0, pacing_epoch = UINT32_MAX;
    static spring::AttentionEvent attention;
    if (pacing_epoch != game.roadshow_epoch) {
        pacing_epoch = game.roadshow_epoch;
        next_offer = 0;
    }
    if (mind_reply_deadline.load() &&
        (!game.s.memory || game.demo != offer_demo || game.s.memory->world_hi != offer_hi ||
         game.s.memory->world_lo != offer_lo)) {
        mind_deadline.store(0);
        mind_reply_deadline.store(0);
        mind_mask.store(0);
        has_pending = false;
    }
    const uint32_t available =
        MindMask(game, voice_idle,
                 now >= next_wish && !social.pending.id && !spring::WishPending(agency) &&
                     agency.current.status != 3);
    const auto trigger = spring::NextAttention(game.Get(), character);
    const bool can_think = voice_idle && GamePowerMode() != spring_power::Mode::Sleep &&
                           game.storage_ok && game.s.memory &&
                           (!game.s.story.enabled || game.s.story.active == spring::kNoStory);
    if (can_think && (available || trigger.kind) && now >= next_offer &&
        now >= mind_reply_deadline.load()) {
        mind_mask.store(available);
        mind_sequence.store(game.s.life.sequence);
        mind_offer.fetch_add(1);
        mind_status.store(1);
        mind_focus_status.store(0);
        mind_social_status.store(0);
        mind_trigger_kind.store(trigger.kind);
        mind_trigger_source.store(trigger.source);
        mind_trigger_target.store(trigger.target);
        attention = trigger;
        offer_demo = game.demo;
        offer_hi = game.s.memory->world_hi;
        offer_lo = game.s.memory->world_lo;
        mind_deadline.store(available ? now + 20000 : 0);
        mind_reply_deadline.store(now + 20000);
        next_offer = now + spring::AttentionPacing(game.roadshow).model_ms;
    }
    if (!available || now >= mind_deadline.load()) {
        mind_mask.store(0);
        mind_deadline.store(0);
    }
    if (now >= mind_reply_deadline.load())
        mind_reply_deadline.store(0);
    char bytes[128];
    // IDF 6.1's nonblocking VFS read reports zero available bytes without the
    // interrupt driver. Keep the existing polling console and read its FIFO;
    // do not install another ISR or change the console's transmit path.
    const int count = usb_serial_jtag_is_driver_installed()
                          ? usb_serial_jtag_read_bytes(bytes, sizeof(bytes), 0)
                          : static_cast<int>(usb_serial_jtag_ll_read_rxfifo(
                                reinterpret_cast<uint8_t*>(bytes), sizeof(bytes)));
    if (count > 0)
        mind_received.fetch_add(count);
    for (int i = 0; i < count; ++i) {
        spring_show::Packet show_packet;
        if (show_parser.Feed(bytes[i], show_packet))
            SubmitRoadshow(show_packet, game, now);
        spring::MindPacket p;
        if (!parser.Feed(bytes[i], p))
            continue;
        pending = p;
        mind_parsed.fetch_add(1);
        has_pending = true;
    }
    if (roadshow_pending.load())
        has_pending = false;
    if (!has_pending)
        return false;
    const auto p = pending;
    const spring::MindReplyGuard guard{mind_boot.load(),
                                       mind_offer.load(),
                                       game.s.memory ? game.s.memory->world_hi : 0,
                                       game.s.memory ? game.s.memory->world_lo : 0,
                                       game.s.life.sequence,
                                       mind_reply_deadline.load()};
    if (!game.s.memory || !guard.Fresh(p, now)) {
        has_pending = false;
        ESP_LOGW(kTag, "MIND_REJECT offer=%lu", static_cast<unsigned long>(p.offer));
        return false;
    }
    const bool body_available = mind_deadline.load() && guard.BodyAvailable(p, now, available);
    if (body_available && game.s.life.cooldown)
        return false;
    has_pending = false;
    bool wish_offered = false;
    const bool needs_permission =
        p.intent && spring::AgencyMode(game.s.story, p.intent, p.target) == 1;
    if (body_available && needs_permission && now >= next_wish && !social.pending.id) {
        auto next = agency;
        wish_offered = spring::OfferWish(game.Get(), next, p.intent, p.target) &&
                       PersistAgency(handle, game.demo, next);
        if (wish_offered) {
            agency = next;
            PublishAgency(agency);
            next_wish = now + spring::AttentionPacing(game.roadshow).wish_ms;
            wish_expires = now + 600000;
            ESP_LOGI(kTag, "WISH token=%lu status=PROPOSED intent=%lu target=%lu",
                     (unsigned long)agency.current.token, (unsigned long)p.intent,
                     (unsigned long)p.target);
        }
    }
    const bool accepted = body_available && !needs_permission &&
                          (p.intent ? game.RequestLocalMindLife(p.intent, p.target) : true);
    if (accepted && !p.intent) {
        game.s.life.cooldown = spring::AttentionPacing(game.roadshow).life_ms;
        ++game.s.revision;
    }
    bool offered = false;
    if (p.social_kind && !spring::WishPending(agency)) {
        auto next = social;
        offered = spring::OfferSocialEvidence(game.Get(), p.social_kind, p.social_record, next,
                                              &character) &&
                  PersistSocial(handle, game.demo, next);
        if (offered) {
            social = next;
            PublishSocial(social);
        }
        ESP_LOGI(kTag, "SOCIAL event=%lu source_kind=%lu source_id=%lu status=%s",
                 static_cast<unsigned long>(offered ? social.pending.id : 0),
                 static_cast<unsigned long>(p.social_kind),
                 static_cast<unsigned long>(p.social_record), offered ? "PENDING" : "REJECTED");
    }
    mind_social_status.store(p.social_kind ? (offered ? 1 : 2) : 0);
    bool character_accepted = false;
    if (p.extended) {
        auto next = character;
        const bool valid = spring::SetCharacterFocus(game.Get(), next, p.focus_mode, p.focus_kind,
                                                     p.focus_source, p.focus_target);
        if (valid && p.focus_mode == 2 &&
            (next.focus_kind != character.focus_kind ||
             next.focus_source != character.focus_source ||
             next.focus_target != character.focus_target)) {
            next.focus_trigger_kind = attention.kind;
            next.focus_trigger_source = attention.source;
            next.focus_since_second = game.s.game_ms / 1000;
        }
        if (valid)
            spring::MarkAttention(next, attention);
        const bool changed = std::memcmp(&next, &character, sizeof(next)) != 0;
        character_accepted = valid && (!changed || PersistCharacter(handle, game.demo, next));
        mind_focus_status.store(character_accepted ? 1 : valid ? 3 : 2);
        if (character_accepted) {
            character = next;
            PublishCharacter(character);
        }
        ESP_LOGI(kTag, "CHARACTER focus_mode=%lu focus_kind=%lu source=%lu status=%lu trigger=%lu",
                 static_cast<unsigned long>(p.focus_mode), static_cast<unsigned long>(p.focus_kind),
                 static_cast<unsigned long>(p.focus_source),
                 static_cast<unsigned long>(mind_focus_status.load()),
                 static_cast<unsigned long>(attention.kind));
    }
    if (accepted && p.intent)
        ESP_LOGI(kTag, "LIFE_START source=LLM offer=%lu sequence=%lu",
                 static_cast<unsigned long>(p.offer),
                 static_cast<unsigned long>(game.s.life.sequence));
    // Historical sharing does not reserve the body. Work may invalidate a move
    // while the same fresh reply still offers a verified, completed experience.
    mind_status.store(wish_offered ? 6
                      : accepted   ? (p.intent ? 2 : 3)
                                   : (offered              ? 4
                                      : character_accepted ? 5
                                                           : 1));
    mind_deadline.store(0);
    mind_reply_deadline.store(0);
    mind_mask.store(0);
    mind_sequence.store(game.s.life.sequence);
    ESP_LOGI(
        kTag, "%s offer=%lu intent=%lu target=%lu life_seq=%lu social_only=%u",
        accepted || wish_offered || offered || character_accepted ? "MIND_ACCEPT" : "MIND_REJECT",
        static_cast<unsigned long>(p.offer), static_cast<unsigned long>(p.intent),
        static_cast<unsigned long>(p.target), static_cast<unsigned long>(game.s.life.sequence),
        offered && !accepted);
    return accepted || wish_offered || offered || character_accepted;
}
// The task owns these five bounded transaction copies for its entire lifetime.
// Keeping them on the stack made the nested save-restore path overflow 24 KiB.
// Allocate once in PSRAM; never allocate transaction buffers in an audio callback.
struct WorkerWorlds {
    spring::Core current, durable, before, candidate, other;
};
struct FreeWorkerWorlds {
    void operator()(WorkerWorlds* p) const {
        if (p) {
            p->~WorkerWorlds();
            heap_caps_free(p);
        }
    }
};
void Worker(void*) {
    void* storage = heap_caps_malloc(sizeof(WorkerWorlds), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!storage) {
        ESP_LOGE(kTag, "World transaction buffers unavailable; game worker not started");
        vTaskDelete(nullptr);
        return;
    }
    std::unique_ptr<WorkerWorlds, FreeWorkerWorlds> worlds(new (storage) WorkerWorlds);
    auto& current = worlds->current;
    auto& durable = worlds->durable;
    ESP_LOGI(kTag, "WORLD_WORKSPACE psram_bytes=%u task_stack_bytes=24576",
             static_cast<unsigned>(sizeof(WorkerWorlds)));
    mind_boot.store(esp_random() | 1u);
    mind_input_ready = true;
    nvs_handle_t handle = 0;
    const auto initialized = nvs_flash_init_partition("game_nvs");
    bool loaded =
        initialized == ESP_OK &&
        nvs_open_from_partition("game_nvs", "spring_v46", NVS_READWRITE, &handle) == ESP_OK &&
        LoadGame(handle, false, current);
    ESP_LOGI(kTag, "GAME_STORAGE partition=game_nvs init=%s loaded=%d",
             esp_err_to_name(initialized), loaded);
    current.storage_ok = loaded;
    if (loaded) {
        RecoverFormalRoadshow(current);
        current.RecoverPlantWork();
        current.storage_ok = Persist(handle, current);
    }
    auto social = LoadSocial(handle, current.Get());
    auto character = LoadCharacter(handle, current.Get());
    auto agency = LoadAgency(handle, current.Get());
    const nvs_handle_t formal_handle = handle;
    uint64_t next_wish = agency.next_token ? esp_timer_get_time() / 1000 +
                                                 spring::AttentionPacing(current.roadshow).wish_ms
                                           : 0;
    uint64_t wish_expires = 0;
    {
        std::lock_guard<std::mutex> guard(world_mutex);
        world = current;
    }
    bool sntp = false;
    uint64_t previous = esp_timer_get_time() / 1000, last_save = previous, last_log = previous;
    uint32_t saved_revision = current.storage_ok ? current.s.revision : 0;
    durable = current;
    bool wave_last_frame_seen = false;
    while (true) {
        const uint64_t now = esp_timer_get_time() / 1000;
        const auto delta = static_cast<uint32_t>(std::min<uint64_t>(now - previous, 1000));
        previous = now;
        PollGamePower();
        const auto audio = Application::GetInstance().GetDeviceState();
        if (!sntp && (audio == kDeviceStateIdle || audio == kDeviceStateListening ||
                      audio == kDeviceStateSpeaking)) {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
            config.sync_cb = [](struct timeval*) { utc_ready.store(true); };
            sntp = esp_netif_sntp_init(&config) == ESP_OK;
        }
        current.Step(delta);
        const auto waiting_at = voice_wait_at.load();
        const auto voice = audio == kDeviceStateSpeaking || audio == kDeviceStateNotifying
                               ? spring_gardener::Voice::Speak
                           : waiting_at && now - waiting_at < 30000 ? spring_gardener::Voice::Wait
                           : audio == kDeviceStateListening         ? spring_gardener::Voice::Listen
                           : audio == kDeviceStateConnecting        ? spring_gardener::Voice::Wait
                                                                    : spring_gardener::Voice::None;
        if (!spring::SocialMatches(current.Get(), social))
            social = LoadSocial(handle, current.Get());
        if (!spring::CharacterMatches(current.Get(), character))
            character = LoadCharacter(handle, current.Get());
        if (!spring::AgencyMatches(current.Get(), agency)) {
            agency = LoadAgency(handle, current.Get());
            next_wish = now + spring::AttentionPacing(current.roadshow).wish_ms;
            wish_expires = 0;
        }
        if (spring::WishPending(agency) && wish_expires && now >= wish_expires) {
            auto next = agency;
            spring::FinishWish(next, spring::WishStatus::Expired);
            if (PersistAgency(handle, current.demo, next)) {
                agency = next;
                PublishAgency(agency);
            }
        }
        const bool mind_changed = PollMind(
            current, now, audio == kDeviceStateIdle && voice == spring_gardener::Voice::None,
            handle, social, character, agency, next_wish, wish_expires);
        if (ApplyRoadshow(current, handle, formal_handle, social, character, agency, now,
                          audio == kDeviceStateIdle && voice == spring_gardener::Voice::None &&
                              communication_source == CommunicationSource::None,
                          uxQueueMessagesWaiting(commands) == 0, &next_wish, &wish_expires)) {
            durable = current;
            saved_revision = current.s.revision;
            last_save = now;
        }
        const auto life_before = current.s.life;
        current.StepLife(delta, voice != spring_gardener::Voice::None,
                         spring_art::GardenerLifeArrived(current.Get()),
                         !mind_deadline.load() && !roadshow_pending.load());
        if (!life_before.intent && current.s.life.intent &&
            current.s.life.reason != unsigned(spring::LifeReason::PlayerInvitation))
            ESP_LOGI(kTag, "LIFE_START source=RULE sequence=%lu",
                     static_cast<unsigned long>(current.s.life.sequence));
        if (life_before.reason == static_cast<unsigned>(spring::LifeReason::PlayerInvitation) &&
            (life_before.phase != current.s.life.phase ||
             life_before.reached != current.s.life.reached ||
             life_before.intent != current.s.life.intent))
            ESP_LOGI(kTag,
                     "VOICE_ACTION kind=INVITED_LIFE phase=%u->%u reached=%u->%u intent=%u "
                     "voice=%u t_ms=%llu",
                     life_before.phase, current.s.life.phase, life_before.reached,
                     current.s.life.reached, life_before.intent, static_cast<unsigned>(voice),
                     static_cast<unsigned long long>(now));
        current.TickStory(0, voice != spring_gardener::Voice::None);
        spring_art::AdvanceMotion(current.Get(), now, voice != spring_gardener::Voice::None, voice);
        const auto plant_before = current.s.plant_action;
        if (current.PlantWorkActive())
            current.StepPlantWork(delta, spring_gardener::PlantWorkArrived(
                                             current.Get(), spring_art::GardenerPose()));
        if (plant_before.status != current.s.plant_action.status) {
            const auto& a = current.s.plant_action;
            ESP_LOGI(kTag,
                     "VOICE_PLANT id=%lu kind=%lu slot=%lu cycle=%lu status=%s reason=%s "
                     "arrived=%d saved=0 t_ms=%llu",
                     (unsigned long)a.id, (unsigned long)a.kind, (unsigned long)a.slot,
                     (unsigned long)a.cycle, spring::PlantActionStatusName(a.status),
                     spring::PlantActionReasonName(a.reason), a.arrived, (unsigned long long)now);
        }
        if (wave_status.load() == 2) {
            const auto pose = spring_art::GardenerPose();
            wave_last_frame_seen |= pose.mode == spring_gardener::Mode::Wave && pose.frame == 3;
            if (wave_last_frame_seen && pose.mode != spring_gardener::Mode::Wave) {
                wave_status.store(3);
                ESP_LOGI(kTag,
                         "VOICE_ACTION id=%lu kind=WAVE_HAND status=COMPLETED voice=%u t_ms=%llu",
                         static_cast<unsigned long>(wave_request_id.load()),
                         static_cast<unsigned>(voice), static_cast<unsigned long long>(now));
            }
        }
        if (utc_ready.load())
            current.Sync(static_cast<int64_t>(time(nullptr)));
        bool changed = mind_changed || (plant_before.status != current.s.plant_action.status);
        if (!current.s.story.enabled && PowerMotionAllowed() && current.storage_ok &&
            now - tilt_at.load() < 200)
            changed |= current.Tilt(tilt_angle.load(), tilt_stable.load(), delta);
        else
            current.Tilt(0, false, delta);
        if (changed)
            PowerActivity("tilt");
        Command command;
        bool ring = false, connect = false;
        if (xQueueReceive(commands, &command, 0) == pdTRUE) {
            worlds->before = current;
            const auto& before = worlds->before;
            bool accepted = false, visual_only = false, wish_approval = false;
            if (command.context_epoch == current.roadshow_epoch ||
                command.action == Action::PlantWork || command.action == Action::CancelActivity)
                switch (command.action) {
                    case Action::Wake:
                        accepted = current.Wake();
                        break;
                    case Action::Shake: {
                        if (current.demo && collection_practice.load()) {
                            collection_shakes.fetch_add(1);
                            accepted = visual_only = true;
                            break;  // Preview never calls Core::Shake or grants resources.
                        }
                        const auto rev = current.s.revision;
                        // Keep the active job and tool; only idle gestures may harvest.
                        if (spring_gardener::CanHarvestOnShake(current.Get(), voice))
                            current.Shake();
                        accepted = current.s.revision != rev;
                        if (!accepted) {
                            accepted = spring_art::ReactShake(now, esp_random(), voice);
                            visual_only = true;
                        }
                        break;
                    }
                    case Action::Select:
                        accepted = !current.PlantWorkActive() &&
                                   current.Select(command.value / 4,
                                                  static_cast<spring::Plant>(command.value % 4));
                        break;
                    case Action::Build:
                        accepted = current.BuildTree();
                        break;
                    case Action::Accept:
                        accepted = audio == kDeviceStateIdle && current.Accept(command.value);
                        connect = accepted;
                        break;
                    case Action::Announce:
                        accepted =
                            audio == kDeviceStateIdle &&
                            command.value == spring_ui::NextCommunication(current.s.notices) &&
                            current.Announce(command.value);
                        ring = accepted;
                        break;
                    case Action::Propose:
                        accepted = current.Propose(command.value, command.argument, command.extra,
                                                   command.actor);
                        if (accepted)
                            economy_offer_turn = user_turn.load();
                        break;
                    case Action::ConfirmProposal:
                        accepted = (command.actor != spring::MemoryActor::VoiceRequest ||
                                    spring::FreshVoiceConfirmation(user_turn.load(),
                                                                   economy_offer_turn)) &&
                                   current.ConfirmProposal(command.value);
                        break;
                    case Action::DismissProposal:
                        accepted = current.DismissProposal(command.value);
                        break;
                    case Action::InviteLife:
                        accepted =
                            (audio == kDeviceStateListening || audio == kDeviceStateSpeaking) &&
                            command.extra == current.s.life.sequence &&
                            current.InviteLife(command.value, command.argument, command.actor);
                        ESP_LOGI(kTag,
                                 "VOICE_ACTION kind=INSPECT_OR_LIFE status=%s intent=%u target=%u "
                                 "voice=%u t_ms=%llu",
                                 accepted ? "ACCEPTED" : "REJECTED", command.value,
                                 command.argument, static_cast<unsigned>(voice),
                                 static_cast<unsigned long long>(now));
                        break;
                    case Action::ResumeWork:
                        accepted =
                            (audio == kDeviceStateListening || audio == kDeviceStateSpeaking) &&
                            command.extra == current.s.life.sequence && current.ResumeWork();
                        ESP_LOGI(
                            kTag,
                            "VOICE_ACTION kind=RESUME_WORK status=%s pending=%u work=%u voice=%u "
                            "t_ms=%llu",
                            accepted ? "ACCEPTED" : "REJECTED", current.LifeWorkPending(),
                            static_cast<unsigned>(current.work), static_cast<unsigned>(voice),
                            static_cast<unsigned long long>(now));
                        break;
                    case Action::PlantWork: {
                        spring::PlantAction receipt{command.token, command.value, command.argument,
                                                    command.extra};
                        receipt.status = spring::PlantActionStatus::Rejected;
                        receipt.reason =
                            current.CheckPlantWork(command.value, command.argument, command.extra);
                        if (command.demo != current.demo ||
                            (current.s.memory && (command.world_hi != current.s.memory->world_hi ||
                                                  command.world_lo != current.s.memory->world_lo)))
                            receipt.reason = spring::PlantActionReason::StaleCycle;
                        else if (audio != kDeviceStateListening && audio != kDeviceStateSpeaking)
                            receipt.reason = spring::PlantActionReason::SessionClosed;
                        else if (current.PlantWorkActive())
                            receipt.reason = spring::PlantActionReason::Busy;
                        accepted = receipt.reason == spring::PlantActionReason::None &&
                                   current.RequestPlantWork(command.token, command.value,
                                                            command.argument, command.extra);
                        if (!accepted)
                            PublishPlantWork(receipt);
                        ESP_LOGI(kTag,
                                 "VOICE_PLANT id=%lu kind=%u slot=%u cycle=%u status=%s reason=%s "
                                 "t_ms=%llu",
                                 (unsigned long)command.token, command.value, command.argument,
                                 command.extra, accepted ? "QUEUED" : "REJECTED",
                                 spring::PlantActionReasonName(receipt.reason),
                                 (unsigned long long)now);
                        break;
                    }
                    case Action::CancelActivity:
                        accepted =
                            (audio == kDeviceStateListening || audio == kDeviceStateSpeaking) &&
                            command.demo == current.demo && current.s.memory &&
                            command.world_hi == current.s.memory->world_hi &&
                            command.world_lo == current.s.memory->world_lo &&
                            current.CancelGardenerActivity(command.value, command.argument);
                        break;
                    case Action::Wave:
                        accepted = wave_request_id.load() == command.value &&
                                   voice != spring_gardener::Voice::None &&
                                   current.work == spring::Work::Rest &&
                                   spring_art::GardenerWave(now);
                        wave_status.store(accepted ? 2 : 4);
                        wave_last_frame_seen = false;
                        ESP_LOGI(
                            kTag, "VOICE_ACTION id=%u kind=WAVE_HAND status=%s voice=%u t_ms=%llu",
                            command.value, accepted ? "RUNNING" : "REJECTED",
                            static_cast<unsigned>(voice), static_cast<unsigned long long>(now));
                        break;
                    case Action::DemoJourney:
                        if (current.demo && current.storage_ok && !current.s.economy.journey) {
                            current.s.economy.journey_clock = 1799000;
                            current.Add(spring::Event::PlanChanged, 254);
                            accepted = true;
                        }
                        break;
                    case Action::Mute:
                        if (current.storage_ok) {
                            current.s.sound_muted = !current.s.sound_muted;
                            ++current.s.revision;
                            accepted = true;
                        }
                        break;
                    case Action::WishOpen: {
                        auto next = agency;
                        accepted = current.storage_ok && audio == kDeviceStateIdle &&
                                   spring::AgencyMatches(current.Get(), next) &&
                                   spring::PresentWish(next, command.value) &&
                                   PersistAgency(handle, current.demo, next);
                        if (accepted) {
                            agency = next;
                            PublishAgency(agency);
                            wish_presented_turn = user_turn.load();
                            wish_ui_request.store(true);
                            visual_only = true;
                        }
                        break;
                    }
                    case Action::WishReply: {
                        const bool from_voice = command.actor == spring::MemoryActor::VoiceRequest;
                        const spring::WishReplyContext reply{
                            from_voice,
                            communication_source == CommunicationSource::Life &&
                                active_call.load() == 98 && !communication_closing.load() &&
                                (audio == kDeviceStateListening || audio == kDeviceStateSpeaking),
                            user_turn.load(), wish_presented_turn};
                        const auto decision = spring::DecideWishReply(
                            current.Get(), agency, command.value, command.argument, reply);
                        if (decision == spring::WishReplyDecision::AlreadyExecuting) {
                            accepted = visual_only =
                                true;  // Retry acknowledgement, never duplicate execution.
                            break;
                        }
                        if (decision == spring::WishReplyDecision::Rejected)
                            break;
                        auto next = agency;
                        if (command.argument == 1) {
                            auto& candidate = worlds->candidate;
                            candidate = current;
                            if (!spring::StageWishApproval(candidate, next, command.value,
                                                           command.actor))
                                break;
                            // Persist permission before any physical action can begin. If the game
                            // commit fails, this becomes interrupted, never a false completion.
                            if (!PersistAgency(handle, current.demo, next))
                                break;
                            current = candidate;
                            wish_approval = true;
                        } else if (command.argument == 2 || command.argument == 3) {
                            spring::FinishWish(next, command.argument == 2
                                                         ? spring::WishStatus::Deferred
                                                         : spring::WishStatus::Declined);
                            if (!PersistAgency(handle, current.demo, next))
                                break;
                            visual_only = true;
                        } else
                            break;
                        agency = next;
                        PublishAgency(agency);
                        accepted = true;
                        ESP_LOGI(kTag, "WISH token=%u status=%s sequence=%lu chain=%lu",
                                 command.value, spring::WishStatusName(agency.current.status),
                                 (unsigned long)agency.current.sequence,
                                 (unsigned long)agency.current.chain);
                        break;
                    }
                    case Action::SocialAccept: {
                        auto next = social;
                        accepted = current.storage_ok && audio == kDeviceStateIdle &&
                                   spring::SocialMatches(current.Get(), next) &&
                                   spring::AcceptSocial(command.value, next) &&
                                   PersistSocial(handle, current.demo, next);
                        if (accepted) {
                            social = next;
                            PublishSocial(social);
                            social_ui_request.store(true);
                            visual_only =
                                true;  // Sidecar already committed, no economic transaction.
                        }
                        ESP_LOGI(kTag, "SOCIAL event=%u status=%s", command.value,
                                 accepted ? "ACCEPTED" : "REJECTED");
                        break;
                    }
                    case Action::Story: {
                        const auto verb = static_cast<spring::StoryVerb>(command.value);
                        const bool voice_request =
                            command.actor == spring::MemoryActor::VoiceRequest;
                        switch (verb) {
                            case spring::StoryVerb::Accept:
                                accepted = (audio == kDeviceStateIdle || voice_request) &&
                                           current.AcceptStory(command.argument);
                                connect = accepted && audio == kDeviceStateIdle;
                                break;
                            case spring::StoryVerb::Inspect:
                                accepted = current.AcceptStory(command.argument);
                                break;
                            case spring::StoryVerb::EndCommunication:
                                accepted = current.EndStoryCommunication(command.argument);
                                break;
                            case spring::StoryVerb::Complete:
                                accepted = current.CompleteStory(command.argument, command.extra);
                                break;
                            case spring::StoryVerb::Pause:
                                accepted = current.PauseStory(command.argument);
                                break;
                            case spring::StoryVerb::Archive:
                                accepted = current.ReadStoryArchive(command.argument);
                                break;
                            case spring::StoryVerb::OfferFlower: {
                                const bool pending = current.s.story.offered;
                                accepted = current.OfferStoryFlower();
                                if (accepted && !pending)
                                    flower_offer_turn = user_turn.load();
                                break;
                            }
                            case spring::StoryVerb::ApproveFlower:
                                accepted =
                                    (!voice_request || spring::FreshVoiceConfirmation(
                                                           user_turn.load(), flower_offer_turn)) &&
                                    current.ApproveStoryFlower(command.token);
                                break;
                            case spring::StoryVerb::OfferName: {
                                const auto previous = current.s.story.candidate_token;
                                accepted =
                                    current.ProposeStoryName(command.argument, command.text.data());
                                if (accepted && previous != current.s.story.candidate_token)
                                    name_offer_turn = user_turn.load();
                                break;
                            }
                            case spring::StoryVerb::ConfirmName:
                                accepted =
                                    (!voice_request || spring::FreshVoiceConfirmation(
                                                           user_turn.load(), name_offer_turn)) &&
                                    current.ConfirmStoryName(command.token);
                                break;
                            case spring::StoryVerb::Observe:
                                accepted = current.NoticeStoryLife();
                                // A repeat observation may record attention without
                                // opening a new chapter. Only a real active chapter
                                // has an opening for the voice handoff.
                                connect = accepted &&
                                          current.s.story.active < spring::kStoryCount &&
                                          audio == kDeviceStateIdle;
                                break;
                            case spring::StoryVerb::Announce:
                                accepted = current.s.story.pending == command.argument &&
                                           current.s.story.announced_node != command.argument;
                                if (accepted) {
                                    current.s.story.announced_node = command.argument;
                                    ++current.s.revision;
                                    ring = true;
                                }
                                break;
                            case spring::StoryVerb::Presented:
                                accepted = current.StoryPresented(command.argument);
                                break;
                            case spring::StoryVerb::ArchiveSeen:
                                accepted = current.StoryArchiveSeen(command.argument);
                                break;
                            case spring::StoryVerb::InviteSeat:
                                accepted = !voice_request && current.ChapterCanOfferSeat() &&
                                           current.InviteLife(3, 0, spring::MemoryActor::Player);
                                break;
                            case spring::StoryVerb::ShowLife:
                                accepted = current.ShowStoryLife(command.argument);
                                break;
                            case spring::StoryVerb::CancelName:
                                accepted = current.s.story.candidate_who &&
                                           command.token == current.s.story.candidate_token;
                                if (accepted) {
                                    current.s.story.candidate_who = 0;
                                    current.s.story.candidate = {};
                                    ++current.s.revision;
                                }
                                break;
                        }
                        break;
                    }
                    case Action::Mode:
                        if (current.roadshow)
                            break;  // Only the guarded roadshow return may change its namespace.
                        if (current.PlantWorkActive()) {
                            current.FinishPlantWork(spring::PlantActionStatus::Cancelled,
                                                    spring::PlantActionReason::Interrupted);
                            PublishPlantWork(current.s.plant_action);
                        }
                        if (current.storage_ok && Persist(handle, current)) {
                            auto& other = worlds->other;
                            if (LoadGame(handle, !current.demo, other) && Persist(handle, other)) {
                                current = other;
                                current.RecoverPlantWork();
                                accepted = true;
                            }
                        }
                        break;
                }
            if (accepted) {
                if (!visual_only && current.LifeActive() && current.LifeWorkPending())
                    current.CoordinateLifeWork();
                if (visual_only || Persist(handle, current)) {
                    if (!visual_only) {
                        saved_revision = current.s.revision;
                        last_save = now;
                        changed = false;
                        durable = current;
                    }
                    if ((command.action == Action::InviteLife ||
                         command.action == Action::ResumeWork ||
                         command.action == Action::PlantWork) &&
                        spring::WishPending(agency)) {
                        auto next = agency;
                        spring::FinishWish(next, spring::WishStatus::Superseded);
                        if (PersistAgency(handle, current.demo, next)) {
                            agency = next;
                            PublishAgency(agency);
                        }
                    }
                    if (command.action == Action::Shake && !visual_only)
                        spring_art::ReactShake(now, esp_random(), voice, true);
                    if (command.action == Action::Propose)
                        proposal_request.store(current.s.economy.proposal.token);
                    feedback.store(visual_only ? 14
                                   : command.action == Action::Shake
                                       ? (before.s.story_stage ? 2 : 5)
                                       : 1);
                    if (command.action == Action::Story &&
                        static_cast<spring::StoryVerb>(command.value) ==
                            spring::StoryVerb::Archive) {
                        // Publish archive ID and voice handoff together across tasks.
                        story_ui_request.store((command.argument + 1) |
                                               (command.extra == 1 ? 0x100u : 0u));
                    }
                    if (ring) {
                        opening_request.store(command.value);
                        if (!current.s.sound_muted)
                            Application::GetInstance().Schedule([] {
                                if (GamePowerMode() == spring_power::Mode::Play &&
                                    Application::GetInstance().GetDeviceState() ==
                                        kDeviceStateIdle) {
                                    Application::GetInstance().PlaySound(Lang::Sounds::OGG_POPUP);
                                    // Notification only; voice starts after explicit acceptance.
                                }
                            });
                    }
                    if (connect) {
                        story_response_node.store(spring::kNoStory);
                        communication_story_node.store(command.action == Action::Story
                            ? current.s.story.active : spring::kNoStory);
                        active_call.store(
                            command.action == Action::Story
                                ? (current.s.story.active < spring::kStoryCount ? 6 : 99)
                                : command.value);
                        communication_ui_request.store(command.action == Action::Story ? 3 : 2);
                        Application::GetInstance().Schedule([] {
                            auto& app = Application::GetInstance();
                            if (app.GetDeviceState() == kDeviceStateIdle)
                                app.InvokeDeviceEvent("接听通讯");
                        });
                    }
                } else {
                    if (command.action == Action::PlantWork) {
                        auto failed = current.s.plant_action;
                        failed.status = spring::PlantActionStatus::Failed;
                        failed.reason = spring::PlantActionReason::Storage;
                        PublishPlantWork(failed);
                    }
                    current = before;
                    if (wish_approval) {
                        spring::FinishWish(agency, spring::WishStatus::Interrupted,
                                           unsigned(spring::LifeResult::Rebooted));
                        PersistAgency(handle, current.demo, agency);
                        PublishAgency(agency);
                    }
                    current.storage_ok = false;
                    ring = connect = false;
                    feedback.store(11);
                }
            } else if (command.action != Action::Announce &&
                       !(command.action == Action::Story &&
                         (command.value == unsigned(spring::StoryVerb::Announce) ||
                          command.value == unsigned(spring::StoryVerb::ShowLife) ||
                          command.value == unsigned(spring::StoryVerb::Presented)))) {
                const unsigned reason =
                    !current.storage_ok                                               ? 7
                    : command.action == Action::Propose && !current.s.story_stage     ? 8
                    : command.action == Action::Propose && !current.s.economy.enabled ? 9
                    : command.action == Action::Propose                               ? 10
                    : command.action == Action::Shake                                 ? 15
                                                                                      : 3;
                feedback.store(reason);
                ESP_LOGW(kTag,
                         "ACTION_REJECT action=%u value=%u argument=%u extra=%u "
                         "reason=%u storage=%d story=%lu economy=%d proposal_seq=%lu",
                         static_cast<unsigned>(command.action), command.value, command.argument,
                         command.extra, reason, current.storage_ok,
                         static_cast<unsigned long>(current.s.story_stage),
                         current.s.economy.enabled,
                         static_cast<unsigned long>(current.s.economy.proposal_seq));
            }
            if (command.action != Action::Announce || accepted)
                ESP_LOGI(kTag, "ACTION kind=%u value=%u accepted=%d revision=%lu",
                         static_cast<unsigned>(command.action), command.value, accepted,
                         static_cast<unsigned long>(current.s.revision));
        }
        if (current.storage_ok && (changed || now - last_save >= 5000) &&
            current.s.revision != saved_revision) {
            if (Persist(handle, current)) {
                saved_revision = current.s.revision;
                last_save = now;
                durable = current;
            } else {
                auto failed = current.s.plant_action;
                if (failed.id) {
                    failed.status = spring::PlantActionStatus::Failed;
                    failed.reason = spring::PlantActionReason::Storage;
                    PublishPlantWork(failed);
                }
                current = durable;
                current.storage_ok = false;
                current.s.plant_action = failed;
                feedback.store(3);
            }
        }
        if (current.storage_ok && current.s.revision == saved_revision) {
            auto next = character;
            if (spring::RefreshCharacter(current.Get(), next) &&
                PersistCharacter(handle, current.demo, next)) {
                character = next;
                PublishCharacter(character);
                ESP_LOGI(kTag, "CHARACTER_MEMORY invitation=%lu chain=%lu",
                         static_cast<unsigned long>(character.important_invitation.id),
                         static_cast<unsigned long>(character.invitation_chain));
            }
        }
        if (current.storage_ok && current.s.revision == saved_revision) {
            auto next = agency;
            if (spring::RefreshAgency(current.Get(), next) &&
                PersistAgency(handle, current.demo, next)) {
                agency = next;
                PublishAgency(agency);
                ESP_LOGI(kTag, "WISH token=%lu status=%s arrived=%lu sequence=%lu",
                         (unsigned long)agency.current.token,
                         spring::WishStatusName(agency.current.status),
                         (unsigned long)agency.current.reached,
                         (unsigned long)agency.current.sequence);
            }
        }
        if (current.s.plant_action.id && current.storage_ok &&
            (current.s.plant_action.status != spring::PlantActionStatus::Completed ||
             current.s.revision == saved_revision)) {
            PublishPlantWork(current.s.plant_action);
            if (plant_before.status != current.s.plant_action.status &&
                current.s.plant_action.status == spring::PlantActionStatus::Completed)
                ESP_LOGI(
                    kTag,
                    "VOICE_PLANT_COMMIT id=%lu status=COMPLETED saved=1 credited=%lu t_ms=%llu",
                    (unsigned long)current.s.plant_action.id,
                    (unsigned long)current.s.plant_action.amount, (unsigned long long)now);
        }
        {
            std::lock_guard<std::mutex> guard(world_mutex);
            world = current;
        }
        if (now - last_log >= 5000) {
            ESP_LOGI(kTag,
                     "WORLD mode=%s water=%lu energy=%lu fiber=%lu tree=%lu story=%lu events=%lu "
                     "saved=%lu storage=%d sram=%u psram=%u",
                     current.demo ? "demo" : "normal", (unsigned long)current.s.water,
                     (unsigned long)current.s.energy, (unsigned long)current.s.fiber,
                     (unsigned long)current.s.tree_stage, (unsigned long)current.s.story_stage,
                     (unsigned long)current.s.event_count, (unsigned long)saved_revision,
                     current.storage_ok, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            ESP_LOGI(kTag,
                     "ECONOMY schema=3 enabled=%d food=%lu parts=%lu built=%lu journey=%lu "
                     "call_id=%lu proposal=%lu goal=%lu region=%lu emergency=%lu stack_free=%u",
                     current.s.economy.enabled, (unsigned long)current.s.economy.food,
                     (unsigned long)current.s.economy.parts, (unsigned long)current.s.economy.built,
                     (unsigned long)current.s.economy.journey,
                     (unsigned long)current.s.economy.journey_id,
                     (unsigned long)current.s.economy.proposal.kind,
                     (unsigned long)current.s.economy.priorities[0],
                     (unsigned long)current.s.economy.region,
                     (unsigned long)current.s.economy.emergency_work,
                     (unsigned)uxTaskGetStackHighWaterMark(nullptr));
            ESP_LOGI(kTag, "GARDENER state=%s", spring_art::GardenerMode());
            last_log = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Dedicated low-priority observer. Never touches actions, clocks, NVS or audio state.
void MonitorWorker(void*) {
    uint32_t sequence = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        const auto s = GetWorldSnapshot();
        const auto pose = spring_art::GardenerPose();
        spring::MonitorRuntime runtime;
        runtime.uptime_ms = esp_timer_get_time() / 1000;
        runtime.fsm = spring_gardener::Name(pose.mode);
        runtime.voice =
            DeviceStateMachine::GetStateName(Application::GetInstance().GetDeviceState());
        runtime.power = spring_power::Name(GamePowerMode());
        runtime.x = pose.x;
        runtime.y = pose.y;
        runtime.internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        runtime.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        runtime.monitor_stack_free = uxTaskGetStackHighWaterMark(nullptr);
        runtime.audio_json = audio::PlaybackDiagnosticsJson(
            Application::GetInstance().GetAudioService().GetPlaybackDiagnostics());
        runtime.background_storage = spring_art::BackgroundStorageMode();
        const auto background = spring_art::GetBackgroundMemory();
        runtime.background_bytes = background.bytes;
        runtime.background_allocations = background.allocations;
        runtime.background_ready = background.ready;
        runtime.psram_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        runtime.psram_largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        runtime.internal_min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        {
            std::lock_guard<std::mutex> guard(collection_metrics_mutex);
            const auto& m = collection_metrics;
            std::ostringstream o;
            o << "{\"practice_only\":true,\"enabled\":" << m.enabled << ",\"active\":" << m.active
              << ",\"particles\":" << m.particles << ",\"frames\":" << m.frames
              << ",\"batches\":" << m.batches << ",\"hits\":" << m.hits
              << ",\"misses\":" << m.misses << ",\"ignored_shakes\":" << m.ignored_shakes
              << ",\"cancels\":" << m.cancels << ",\"max_update_us\":" << m.max_update_us
              << ",\"pixel_flash_bytes\":24576}";
            runtime.collection_json = o.str();
        }
        runtime.mind_boot = mind_boot.load();
        runtime.mind_offer = mind_offer.load();
        runtime.mind_mask = mind_mask.load();
        runtime.mind_status = mind_status.load();
        runtime.mind_sequence = mind_sequence.load();
        runtime.mind_deadline = mind_deadline.load();
        runtime.mind_reply_deadline = mind_reply_deadline.load();
        runtime.mind_received = mind_received.load();
        runtime.mind_parsed = mind_parsed.load();
        runtime.communication_json = CommunicationJson(s);
        runtime.observation_json = CharacterJson(s, true);
        runtime.roadshow_json = RoadshowJson(s);
        runtime.mind_focus_status = mind_focus_status.load();
        runtime.mind_social_status = mind_social_status.load();
        runtime.mind_trigger_kind = mind_trigger_kind.load();
        runtime.mind_trigger_source = mind_trigger_source.load();
        runtime.mind_trigger_target = mind_trigger_target.load();
        for (unsigned i = 0; i < runtime.queries.size(); ++i)
            runtime.queries[i] = monitor_queries[i].load();
        const auto json = spring::MonitorJson(s, runtime, active_call.load());
        constexpr size_t chunk_bytes = 240;
        const size_t total = (json.size() + chunk_bytes - 1) / chunk_bytes;
        if (total > 512)
            continue;
        ++sequence;
        for (size_t part = 0; part < total; ++part) {
            unsigned char encoded[325]{};
            size_t length = 0;
            const size_t start = part * chunk_bytes;
            if (mbedtls_base64_encode(encoded, sizeof(encoded) - 1, &length,
                                      reinterpret_cast<const unsigned char*>(json.data() + start),
                                      std::min(chunk_bytes, json.size() - start)) != 0)
                break;
            encoded[length] = 0;
            ESP_LOGI("SpringMonitor", "MONITOR_V1 seq=%lu part=%u total=%u data=%s",
                     static_cast<unsigned long>(sequence), static_cast<unsigned>(part),
                     static_cast<unsigned>(total), reinterpret_cast<const char*>(encoded));
        }
    }
}

lv_obj_t *stage, *hud, *caption, *call, *call_dot, *journal_dot, *resource_values[5];
lv_obj_t* collection_status_label = nullptr;
lv_obj_t* collection_close_label = nullptr;
lv_obj_t *modal_backdrop, *modal, *modal_title, *modal_body, *modal_hint, *primary, *choices[3],
    *choice_labels[3], *notice_text, *back_button, *resource_icon, *plant_tile, *growth_track,
    *growth_fill;
unsigned selected_resource = 0;
uint32_t seen_log_id = 0;
std::vector<lv_obj_t*> text_nodes, small_labels;
struct TouchCheck {
    const char* name;
    lv_obj_t* object;
};
std::vector<TouchCheck> touch_checks;
void LogTouchMap() {
    lv_obj_update_layout(lv_screen_active());
    lv_obj_update_layout(lv_layer_top());
    for (const auto& check : touch_checks) {
        if (!lv_obj_is_visible(check.object))
            continue;
        lv_area_t bounds;
        lv_obj_get_coords(check.object, &bounds);
        bounds.x1 = std::max<int32_t>(0, bounds.x1);
        bounds.x2 = std::min<int32_t>(479, bounds.x2);
        bounds.y1 = std::max<int32_t>(0, bounds.y1);
        bounds.y2 = std::min<int32_t>(479, bounds.y2);
        if (bounds.x1 > bounds.x2 || bounds.y1 > bounds.y2)
            continue;
        lv_point_t point{(bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2};
        auto* hit = lv_indev_search_obj(lv_layer_sys(), &point);
        if (!hit)
            hit = lv_indev_search_obj(lv_layer_top(), &point);
        if (!hit)
            hit = lv_indev_search_obj(lv_screen_active(), &point);
        ESP_LOGI(kTag, "TOUCH_MAP name=%s x=%ld y=%ld hit=%d", check.name,
                 static_cast<long>(point.x), static_cast<long>(point.y), hit == check.object);
    }
}
std::shared_ptr<LvglFont> font_owner;
std::shared_ptr<LvglFont> local_game_font;
unsigned modal_kind = 0, selected_slot = 0, selected_plant = 0, selected_call = 0, log_page = 0;
uint64_t message_until = 0;
lv_obj_t* plot_hits[4];
lv_obj_t* facility_hits[9]{};
lv_obj_t* economy_icon;
unsigned economy_page = 0, proposal_token = 0;
spring::TapGuard dialog_taps;
void EconomyModal(unsigned kind);
void EconomyChoice(unsigned action);
void EconomyConfirm();
lv_obj_t* story_robot_hit = nullptr;
lv_obj_t* social_bubble = nullptr;
lv_obj_t* archive_scroll = nullptr;
lv_obj_t* archive_content = nullptr;
lv_obj_t* archive_text = nullptr;
void RenderEconomyModal(const spring::Snapshot& s);
void StoryModal(unsigned kind);
void StoryChoice(unsigned action);
void StoryConfirm();
void RenderStoryModal(const spring::Snapshot& s);

lv_obj_t* Box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color, int radius = 0) {
    auto* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}
void LabelScale(lv_obj_t* label) {
    const auto* font = font_owner ? font_owner->font() : LV_FONT_DEFAULT;
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
    const bool small =
        std::find(small_labels.begin(), small_labels.end(), label) != small_labels.end();
    const int target = label == modal_title ? 26 : label == modal_hint ? 20 : small ? 22 : 24;
    const int scale =
        (label == collection_status_label || label == collection_close_label)
            ? 256
            : std::min(256, target * 256 / std::max(1, static_cast<int>(font->line_height)));
    lv_obj_set_style_transform_scale_x(label, scale, 0);
    lv_obj_set_style_transform_scale_y(label, scale, 0);
    lv_obj_set_style_transform_pivot_x(label, 0, 0);
    lv_obj_set_style_transform_pivot_y(label, 0, 0);
    const auto width = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(label)));
    lv_obj_set_width(label, width * 256 / scale);
}
lv_obj_t* Text(lv_obj_t* parent, int x, int y, int width, const char* value,
               uint32_t color = 0xEEE4CD) {
    auto* o = lv_label_create(parent);
    spring_ui::Passive(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_user_data(o, reinterpret_cast<void*>(static_cast<intptr_t>(width)));
    lv_label_set_text(o, value);
    text_nodes.push_back(o);
    LabelScale(o);
    return o;
}
lv_obj_t* Button(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                 lv_event_cb_t callback, uintptr_t data = 0) {
    auto* b = spring_ui::Target(parent, {x, y, w, h});
    lv_obj_set_style_bg_color(b, lv_color_hex(0x455E50), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_ext_click_area(b, 2);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x76977A), LV_STATE_PRESSED);
    Text(b, 10, 10, w - 12, text);
    lv_obj_add_event_cb(
        b,
        [](lv_event_t* e) {
            if (modal_kind && !dialog_taps.Accept(esp_timer_get_time()))
                lv_event_stop_processing(e);
        },
        LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(b, callback, LV_EVENT_CLICKED, reinterpret_cast<void*>(data));
    lv_obj_add_event_cb(
        b,
        [](lv_event_t* e) {
            ESP_LOGI(kTag, "TOUCH_TARGET name=%s event=%s",
                     static_cast<const char*>(lv_event_get_user_data(e)),
                     lv_event_get_code(e) == LV_EVENT_PRESSED ? "press" : "click");
        },
        LV_EVENT_PRESSED, const_cast<char*>(text));
    touch_checks.push_back({text, b});
    return b;
}
void RefreshFont() {
    auto* theme = Board::GetInstance().GetDisplay()->GetTheme();
    auto next = local_game_font ? local_game_font : theme ? theme->GetTextFont() : nullptr;
    if (!next || !spring_ui::HasGameChinese(next->font()) || next == font_owner)
        return;
    auto previous = font_owner;
    font_owner = next;
    for (auto* text : text_nodes)
        LabelScale(text);
}
void HideModal(lv_event_t*) {
    if (!modal_kind)
        return;
    if (communication_source != CommunicationSource::None &&
        Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
        communication_closing = true;
        return;
    }
    if (communication_source == CommunicationSource::Story && active_call.load() == 6) {
        const auto node = communication_story_node.load();
        if (node < spring::kStoryCount &&
            !QueueStoryAction(unsigned(spring::StoryVerb::EndCommunication), node, 0, 0, "", false))
            return;  // Keep the card until its closing transaction can be queued.
    }
    story_response_node.store(spring::kNoStory);
    communication_story_node.store(spring::kNoStory);
    story_playback_receipt.Reset();
    active_call.store(99);
    ConsumeClosingTouch();
    if (auto* input = lv_indev_active())
        lv_indev_wait_release(input);
    spring_ui::DialogVisible(modal_backdrop, modal, false);
    modal_kind = 0;
    communication_source = CommunicationSource::None;
    communication_archive.store(255);
    communication_closing = false;
    communication_close_sent = false;
}
void StartCollectionPractice(const spring::Snapshot& s) {
    if (!spring_ui::kCollectionPracticeEnabled || !s.demo || !s.storage_ok ||
        Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
        return;
    if (!collection_view.Layer()) {
        lv_mem_monitor_t pool{};
        lv_mem_monitor(&pool);
        if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 48 * 1024 ||
            pool.free_biggest_size < 64 * 1024) {
            feedback.store(16);
            return;
        }
        collection_view.Create(lv_layer_top(),
                               []() -> uint64_t { return esp_timer_get_time() / 1000; });
        auto* label = Text(collection_view.Layer(), 90, 74, 294, "");
        collection_status_label = label;
        LabelScale(label);  // Native font avoids a large transformed text layer.
        lv_obj_set_style_bg_color(label, lv_color_hex(0x263C32), 0);
        lv_obj_set_style_bg_opa(label, LV_OPA_90, 0);
        auto* close = Button(collection_view.Layer(), 396, 76, 72, 44, "返回", [](lv_event_t*) {
            collection_view.Stop();
            collection_practice.store(false);
            ConsumeClosingTouch();
        });
        collection_close_label = lv_obj_get_child(close, 0);
        LabelScale(collection_close_label);
        spring_ui::PlaceButtonText(close, collection_close_label);
        collection_view.Attach(label, close);
    }
    HideModal(nullptr);
    collection_shakes.store(0);
    collection_view.Start();
    collection_practice.store(true);
}
void Modal(unsigned kind) {
    modal_kind = kind;
    if (economy_icon)
        lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(modal, kind == 6 ? 14 : 53, kind == 6 ? 68 : 78);
    lv_obj_set_size(modal, kind == 6 ? 260 : 374, kind == 6 ? 194 : 333);
    lv_obj_set_pos(modal_body, kind == 6 ? 64 : kind == 1 ? 126 : 20, 57);
    lv_obj_set_user_data(modal_body,
                         reinterpret_cast<void*>(static_cast<intptr_t>(kind == 6   ? 182
                                                                       : kind == 1 ? 224
                                                                                   : 328)));
    const auto small_body = std::find(small_labels.begin(), small_labels.end(), modal_body);
    if ((kind == 6 || kind >= 7) && small_body == small_labels.end())
        small_labels.push_back(modal_body);
    else if (kind != 6 && kind < 7 && small_body != small_labels.end())
        small_labels.erase(small_body);
    LabelScale(modal_body);
    lv_obj_set_size(back_button, kind == 6 ? 90 : 210, kind == 6 ? 46 : 56);
    auto* back_text = lv_obj_get_child(back_button, 0);
    lv_obj_set_user_data(back_text,
                         reinterpret_cast<void*>(static_cast<intptr_t>(kind == 6 ? 78 : 138)));
    LabelScale(back_text);
    lv_obj_set_pos(back_button, kind == 6 ? 156 : 203, kind == 6 ? 146 : 272);
    lv_obj_set_pos(primary, 17, 272);
    if (kind == 1 || kind == 6)
        lv_obj_remove_flag(resource_icon, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(resource_icon, LV_OBJ_FLAG_HIDDEN);
    if (kind == 1) {
        lv_obj_remove_flag(plant_tile, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(growth_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_parent(resource_icon, plant_tile);
        lv_image_set_pivot(resource_icon, 0, 0);
        lv_image_set_scale(resource_icon, 256);
    } else {
        lv_obj_add_flag(plant_tile, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(growth_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_parent(resource_icon, modal);
        lv_image_set_pivot(resource_icon, 0, 0);
        lv_image_set_scale(resource_icon, 256);
        lv_obj_set_pos(resource_icon, 20, 62);
    }
    if (kind == 6)
        lv_obj_add_flag(modal_hint, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(modal_hint, LV_OBJ_FLAG_HIDDEN);
    if (kind != 6) {
        spring_ui::LayoutDialog(modal, primary, back_button, choices);
        lv_obj_set_pos(modal_title, 20, 14);
        lv_obj_set_user_data(modal_title, reinterpret_cast<void*>(350));
        LabelScale(modal_title);
        lv_obj_set_pos(modal_body, kind == 1 ? 126 : 20, 54);
        lv_obj_set_user_data(modal_body, reinterpret_cast<void*>(kind == 1 ? 318 : 424));
        LabelScale(modal_body);
        lv_obj_set_pos(modal_hint, 20, 188);
        lv_obj_set_user_data(modal_hint, reinterpret_cast<void*>(424));
        LabelScale(modal_hint);
        lv_obj_set_pos(economy_icon, 388, 8);
        lv_obj_set_width(growth_track, 318);
    } else {
        lv_obj_set_pos(modal_title, 20, 16);
        lv_obj_set_user_data(modal_title, reinterpret_cast<void*>(220));
        LabelScale(modal_title);
    }
    for (auto* button : {choices[0], choices[1], choices[2], primary, back_button}) {
        auto* label = lv_obj_get_child(button, 0);
        lv_obj_set_user_data(
            label, reinterpret_cast<void*>(static_cast<intptr_t>(lv_obj_get_width(button) - 24)));
        LabelScale(label);
        spring_ui::PlaceButtonText(button, label);
    }
    auto* primary_text = lv_obj_get_child(primary, 0);
    spring_ui::TextChanged(primary_text, kind == 5 ? "演示" : "确认");
    spring_ui::TextChanged(lv_obj_get_child(back_button, 0),
                           communication_source != CommunicationSource::None ? "结束通讯" : "返回");
    for (auto* b : choices)
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(primary, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_state(primary, LV_STATE_DISABLED);
    lv_label_set_text(modal_hint, "");
    if (kind == 5) {
        log_page = 0;
        const auto snapshot =
            std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
        const auto& s = *snapshot;
        if (s.event_count)
            seen_log_id = s.events[s.event_count - 1].id;
        lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
        for (unsigned i = 0; i < 3; ++i) {
            lv_obj_remove_flag(choices[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(choice_labels[i], i == 0 ? "上一" : i == 1 ? "下一" : "提示音");
        }
    }
    if (archive_scroll) {
        if (kind == 19) {
            lv_obj_add_flag(modal_body, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(archive_scroll, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_y(archive_scroll, 0, LV_ANIM_OFF);
        } else {
            lv_obj_remove_flag(modal_body, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(archive_scroll, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (kind == 27) {
        // The same modal becomes a compact call card; the robot stays visible below.
        spring_ui::LayoutCommunicationCard(
            modal, modal_hint, back_button,
            communication_source == CommunicationSource::Life && active_call.load() == 98
                ? choices
                : nullptr);
        lv_obj_set_user_data(modal_title, reinterpret_cast<void*>(370));
        lv_obj_set_user_data(modal_body, reinterpret_cast<void*>(370));
        LabelScale(modal_title);
        LabelScale(modal_body);
        lv_obj_set_user_data(modal_hint, reinterpret_cast<void*>(220));
        LabelScale(modal_hint);
        auto* label = lv_obj_get_child(back_button, 0);
        lv_obj_set_user_data(label, reinterpret_cast<void*>(130));
        LabelScale(label);
        spring_ui::PlaceButtonText(back_button, label);
    }
    spring_ui::DialogVisible(modal_backdrop, modal, true);
}
// Button handlers own their snapshots on the heap: nested UI calls must not
// multiply ~1.9KB value copies on the 7KB LVGL task stack. C++17 direct new
// construction elides the return temporary (make_unique would not).
void ShowPlant(unsigned slot) {
    selected_slot = slot;
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    selected_plant = static_cast<unsigned>((slot < 3 ? s.plots[slot] : s.economy.fourth).next);
    Modal(1);
    for (unsigned i = 0; i < 3; ++i) {
        lv_obj_remove_flag(choices[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(choice_labels[i], i == 0 ? "上一种" : i == 1 ? "下一种" : "经营计划");
    }
    lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
}
void Talk(lv_event_t*) {
    active_call.store(99);
    Application::GetInstance().Schedule([] {
        if (communication_source != CommunicationSource::Player)
            return;
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateIdle ||
            app.GetDeviceState() == kDeviceStateSpeaking ||
            app.GetDeviceState() == kDeviceStateListening)
            app.ToggleChatState();
    });
}
bool OpenCommunicationInvitation(const spring::Snapshot& s, bool include_history) {
    if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
        return false;
    uint32_t wish = 0, social = 0;
    {
        std::lock_guard<std::mutex> guard(social_mutex);
        if (spring::AgencyMatches(s, agency_public) && spring::WishPending(agency_public))
            wish = agency_public.current.token;
        if (spring::SocialMatches(s, social_public))
            social = social_public.pending.id;
    }
    const auto route = spring::SelectCommunication(s, wish, social, include_history);
    switch (route.kind) {
        case spring::CommunicationRouteKind::Story:
            QueueStoryAction(unsigned(spring::StoryVerb::Accept), route.id, 0, 0, "", false);
            break;
        case spring::CommunicationRouteKind::Archive:
            QueueStoryAction(unsigned(spring::StoryVerb::Archive), route.id, 1, 0, "", false);
            break;
        case spring::CommunicationRouteKind::Wish:
            Send(Action::WishOpen, route.id);
            break;
        case spring::CommunicationRouteKind::Social:
            Send(Action::SocialAccept, route.id);
            break;
        default:
            return false;
    }
    ESP_LOGI(kTag, "COMM_ROUTE kind=%u id=%u history=%u", unsigned(route.kind), route.id,
             include_history);
    return true;
}
void OnCall(lv_event_t*) {
    const auto snapshot =
        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
    const auto& s = *snapshot;
    if (!s.story_stage) {
        if (Send(Action::Wake) && s.story.enabled)
            QueueStoryAction(static_cast<unsigned>(spring::StoryVerb::Accept), 0, 0, 0, "", false);
        return;
    }
    if (OpenCommunicationInvitation(s, false))
        return;
    if (s.economy.journey && (s.notices & 16)) {
        selected_call = 4;
        EconomyModal(9);
        return;
    }
    if ((s.notices & 32) && s.economy.proposal.kind) {
        EconomyModal(8);
        return;
    }
    if (!s.notices) {
        communication_source = CommunicationSource::Player;
        Modal(27);
        Talk(nullptr);
        return;
    }
    selected_call = spring_ui::NextCommunication(s.notices);
    Modal(3);
    lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
}
void Confirm(lv_event_t*) {
    if (modal_kind >= 17) {
        StoryConfirm();
        return;
    }
    bool queued = false;
    if (modal_kind >= 7) {
        EconomyConfirm();
        return;
    }
    if (modal_kind == 5) {
        Modal(4);
        lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (modal_kind == 1)
        queued = Send(Action::Propose, 2, selected_slot, selected_plant);
    if (modal_kind == 2)
        queued = Send(Action::Propose, 7);
    if (modal_kind == 3) {
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
            return;
        queued = Send(Action::Accept, selected_call);
    }
    if (modal_kind == 4)
        queued = Send(Action::Mode);
    if (queued)
        HideModal(nullptr);
}
#include "spring_economy_ui.inc"
#include "spring_story_ui.inc"
uint64_t refresh_start_us = 0, max_refresh_us = 0, max_render_gap_us = 0, max_render_work_us = 0,
         previous_render_us = 0;
uint32_t render_count = 0, refresh_count = 0;
struct RenderTiming {
    uint64_t started = esp_timer_get_time();
    RenderTiming() {
        if (previous_render_us)
            max_render_gap_us = std::max(max_render_gap_us, started - previous_render_us);
        previous_render_us = started;
    }
    ~RenderTiming() {
        max_render_work_us = std::max<uint64_t>(max_render_work_us, esp_timer_get_time() - started);
        ++render_count;
        static uint64_t logged = 0;
        if (started - logged >= 5000000) {
            lv_mem_monitor_t memory{};
            lv_mem_monitor(&memory);
            ESP_LOGI(kTag,
                     "UI_PERF renders=%lu refreshes=%lu interval_us=%llu gap_ms=%llu "
                     "callback_us=%llu refresh_ms=%llu modal=%u "
                     "lvgl_free=%u largest=%u frag=%u ui_stack_free=%u",
                     static_cast<unsigned long>(render_count),
                     static_cast<unsigned long>(refresh_count), started - logged,
                     max_render_gap_us / 1000, max_render_work_us, max_refresh_us / 1000,
                     modal_kind, static_cast<unsigned>(memory.free_size),
                     static_cast<unsigned>(memory.free_biggest_size), memory.frag_pct,
                     static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            logged = started;
            render_count = 0;
            refresh_count = 0;
            max_render_gap_us = max_render_work_us = max_refresh_us = 0;
        }
    }
};
void RefreshEvent(lv_event_t* e) {
    const auto now = static_cast<uint64_t>(esp_timer_get_time());
    if (lv_event_get_code(e) == LV_EVENT_REFR_START)
        refresh_start_us = now;
    else if (refresh_start_us) {
        ++refresh_count;
        max_refresh_us = std::max(max_refresh_us, now - refresh_start_us);
        refresh_start_us = 0;
    }
}
void Render(lv_timer_t* timer) {
    const auto power = GamePowerMode();
    lv_timer_set_period(timer, power == spring_power::Mode::Play ? 50 : 250);
    if (power == spring_power::Mode::Sleep) {
        if (collection_view.Enabled()) {
            collection_view.Stop();
            collection_practice.store(false);
            std::lock_guard<std::mutex> guard(collection_metrics_mutex);
            collection_metrics = collection_view.GetMetrics();
        }
        return;
    }
    const auto receipt_audio = Application::GetInstance().GetDeviceState();
    const auto delivered_node = story_playback_receipt.Poll(
        story_response_node.load(), receipt_audio == kDeviceStateSpeaking,
        receipt_audio == kDeviceStateListening &&
            Application::GetInstance().GetAudioService().IsPlaybackIdle());
    if (delivered_node < spring::kStoryCount &&
        !QueueStoryAction(unsigned(spring::StoryVerb::Presented), delivered_node, 0, 0, "", false))
        story_playback_receipt.Reset();
    if (communication_source == CommunicationSource::Story && active_call.load() == 6 &&
        receipt_audio == kDeviceStateIdle &&
        story_playback_receipt.delivered < spring::kStoryCount)
        HideModal(nullptr);  // A real hangup also closes its delivered invitation.
    if (communication_closing) {
        const auto audio = Application::GetInstance().GetDeviceState();
        if (audio == kDeviceStateIdle)
            HideModal(nullptr);
        else if (audio == kDeviceStateListening && !communication_close_sent &&
                 Application::GetInstance().GetAudioService().IsPlaybackIdle()) {
            communication_close_sent = true;
            Application::GetInstance().Schedule([] {
                if (Application::GetInstance().GetDeviceState() == kDeviceStateListening &&
                    Application::GetInstance().GetAudioService().IsPlaybackIdle())
                    Application::GetInstance().ToggleChatState();
                else
                    communication_close_sent.store(false);
            });
        }
    }
    RenderTiming timing;
    RefreshFont();
    static auto snapshot = std::make_unique<spring::Snapshot>();
    GetWorldSnapshot(*snapshot);
    const auto& s = *snapshot;
    static uint32_t displayed_show_epoch = 0;
    spring_ui::SyncSceneInput(s.roadshow_epoch, displayed_show_epoch, roadshow_input_epoch, [] {
        ConsumeClosingTouch();  // Also discard main-scene gestures when no modal is open.
        HideModal(nullptr);
        seen_log_id = 0;
        feedback.store(0);
    });
    const auto now = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    if (collection_view.Enabled()) {
        const bool ordinary_call =
            modal_kind == 27 && communication_source == CommunicationSource::Player;
        if (!s.demo || (modal_kind && !ordinary_call)) {
            collection_view.Stop();
            collection_practice.store(false);
        } else {
            const auto begun = esp_timer_get_time();
            auto shakes = std::min(8u, collection_shakes.exchange(0));
            while (shakes--)
                collection_view.Shake(now);
            const auto audio = Application::GetInstance().GetDeviceState();
            collection_view.Update(now, ordinary_call || audio == kDeviceStateListening ||
                                            audio == kDeviceStateSpeaking ||
                                            audio == kDeviceStateConnecting);
            collection_view.RecordUpdateUs(esp_timer_get_time() - begun);
        }
    }
    {
        std::lock_guard<std::mutex> guard(collection_metrics_mutex);
        collection_metrics = collection_view.GetMetrics();
    }
    const auto requested = proposal_request.load();
    if (requested && requested == s.economy.proposal.token && power == spring_power::Mode::Play) {
        proposal_request.store(0);
        EconomyModal(8);
    }
    const bool dialog_open = !lv_obj_has_flag(modal, LV_OBJ_FLAG_HIDDEN);
    spring::SocialState social;
    {
        std::lock_guard<std::mutex> guard(social_mutex);
        social = social_public;
    }
    if (social_bubble) {
        const bool valid = !dialog_open && !collection_practice.load() &&
                             (!s.story.enabled || s.story.active == spring::kNoStory) &&
                             (spring::StoryCommunicationNode(s) < spring::kStoryCount ||
                              (spring::SocialMatches(s, social) && social.pending.id) ||
                              [&] {
                                  std::lock_guard<std::mutex> guard(social_mutex);
                                  return spring::AgencyMatches(s, agency_public) &&
                                         spring::WishPending(agency_public);
                              }()) &&
                             Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
        const auto pose = spring_art::GardenerPose();
        spring_ui::PlaceInvitationBubble(
            social_bubble, valid, spring::CommunicationBubbleIdle(s),
            std::clamp(static_cast<int>(pose.x) - 32, 75, 342),
            std::clamp(static_cast<int>(pose.y) - 88, 110, 330));
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (i < spring_art::PlotCount(s)) {
            lv_obj_remove_flag(plot_hits[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(plot_hits[i], spring_art::PlotX(s, i) - 49, 335);
        } else
            lv_obj_add_flag(plot_hits[i], LV_OBJ_FLAG_HIDDEN);
    }
    const uint32_t resource_amounts[] = {s.fiber, s.water, s.energy, s.economy.food,
                                         s.economy.parts};
    for (unsigned i = 0; i < 5; ++i)
        spring_ui::TextFormat(resource_values[i], "%lu",
                              static_cast<unsigned long>(resource_amounts[i]));
    const bool recorder_attention =
        s.economy.proposal.kind || s.story.pending < spring::kStoryCount ||
        s.story.paused < spring::kStoryCount || s.story.candidate_who ||
        (s.story.offered && !s.story.flower) || (s.story.archives & ~s.story.archives_read);
    if (recorder_attention)
        lv_obj_remove_flag(journal_dot, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(journal_dot, LV_OBJ_FLAG_HIDDEN);
    const auto story_contact = spring::StoryCommunicationNode(s);
    bool wish_pending = false;
    {
        std::lock_guard<std::mutex> guard(social_mutex);
        wish_pending = spring::AgencyMatches(s, agency_public) && spring::WishPending(agency_public);
    }
    const bool pending = s.notices != 0 || story_contact < spring::kStoryCount || wish_pending ||
                         (spring::SocialMatches(s, social) && social.pending.id);
    if (!dialog_open && story_contact < spring::kStoryCount &&
        s.story.pending == story_contact && s.story.announced_node != story_contact &&
        power == spring_power::Mode::Play &&
        Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
        QueueStoryAction(unsigned(spring::StoryVerb::Announce), story_contact, 0, 0, "", false);
    if (!dialog_open || communication_source != CommunicationSource::None) {
        if (s.story.enabled && power == spring_power::Mode::Play && s.life.phase == 3 &&
            s.life.reached) {
            const unsigned bit = s.life.intent == 3   ? 1
                                 : s.life.target == 4 ? 2
                                 : s.life.target == 5 ? 4
                                                      : 8;
            if (!(s.story.life_shown & bit))
                QueueStoryAction(static_cast<unsigned>(spring::StoryVerb::ShowLife),
                                 s.life.sequence, 0, 0, "", false);
        }
        const auto audio = Application::GetInstance().GetDeviceState();
        const bool speaking = audio == kDeviceStateSpeaking || audio == kDeviceStateNotifying;
        if (story_robot_hit) {
            const bool enabled = s.story.enabled && s.life.phase == 3 && s.life.reached;
            if (enabled) {
                const auto pose = spring_art::GardenerPose();
                lv_obj_set_pos(story_robot_hit, static_cast<int>(pose.x) - 32,
                               static_cast<int>(pose.y) - 40);
                lv_obj_remove_flag(story_robot_hit, LV_OBJ_FLAG_HIDDEN);
            } else
                lv_obj_add_flag(story_robot_hit, LV_OBJ_FLAG_HIDDEN);
        }
        spring_art::Update(s, now, audio == kDeviceStateListening || speaking,
                           power == spring_power::Mode::Quiet, speaking,
                           !dialog_open && !collection_view.Enabled());
        lv_obj_update_layout(spring_art::World());
        spring_ui::SyncFacilityTargets(facility_hits);
        spring_ui::ColorChanged(call, lv_color_hex(pending ? 0x6D7D37 : 0x354A35));
        const auto border_color = lv_color_hex(pending ? 0xF5D482 : 0xC4C6A1);
        if (!lv_color_eq(lv_obj_get_style_border_color(call, LV_PART_MAIN), border_color))
            lv_obj_set_style_border_color(call, border_color, 0);
        if (pending)
            lv_obj_remove_flag(call_dot, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(call_dot, LV_OBJ_FLAG_HIDDEN);
    }
    if (!dialog_open && pending && power == spring_power::Mode::Play &&
        Application::GetInstance().GetDeviceState() == kDeviceStateIdle) {
        const auto bit = spring_ui::NextCommunication(s.notices);
        if (bit < 6 && !(s.announced & (1u << bit)))
            Send(Action::Announce, bit);
    }
    const auto msg = feedback.exchange(0);
    static const char* operation_error = nullptr;
    if (msg) {
        operation_error = msg == 6    ? "请求较多，请稍后再点一次"
                          : msg == 7  ? "存档暂不可用，已停止操作以保护进度"
                          : msg == 8  ? "请先点击通讯唤醒园丁，再查看建设"
                          : msg == 9  ? "经营系统未启用，请检查设备版本"
                          : msg == 10 ? "方案参数异常，请返回后重新选择"
                          : msg == 11 ? "保存失败，本次未执行；请勿重置存档"
                          : msg == 16 ? "当前内存紧张，接取练习暂未开启"
                                      : nullptr;
    }
    if (msg || picked_up.exchange(false)) {
        spring_ui::TextChanged(notice_text, operation_error ? operation_error
                                            : msg == 3      ? "操作未完成，请检查状态"
                                            : msg == 14     ? "园丁回应了你"
                                            : msg == 15     ? "园丁已经注意到你了"
                                            : msg == 4      ? "还没有可摇落的成熟植物"
                                            : msg == 5      ? "园丁苏醒了"
                                            : msg == 2      ? "已收获成熟植物"
                                            : msg == 1      ? "已保存"
                                                            : "你回来了");
        lv_obj_remove_flag(notice_text, LV_OBJ_FLAG_HIDDEN);
        message_until = now + (operation_error ? 6000 : 2200);
    }
    const auto opening = opening_request.exchange(99);
    if (opening < 6) {
        spring_ui::TextChanged(notice_text, spring::Opening(opening, s));
        lv_obj_remove_flag(notice_text, LV_OBJ_FLAG_HIDDEN);
        message_until = now + 8000;
    }
    if (now > message_until) {
        lv_obj_add_flag(notice_text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
    }
    static unsigned traced_modal = UINT32_MAX;
    if (traced_modal != modal_kind) {
        LogTouchMap();
        traced_modal = modal_kind;
    }
    const auto current_audio = Application::GetInstance().GetDeviceState();
    const auto story_requested = current_audio == kDeviceStateIdle ||
                                         current_audio == kDeviceStateListening ||
                                         current_audio == kDeviceStateSpeaking
                                     ? story_ui_request.exchange(0)
                                     : 0;
    if (story_requested) {
        story_doc = (story_requested & 0xFFu) - 1;
        const bool start_voice = story_requested & 0x100u;
        if (start_voice) {
            communication_source = CommunicationSource::Story;
            communication_archive.store(story_doc);
            active_call.store(99);
        }
        StoryModal(19);
        if (start_voice) {
            ESP_LOGI(kTag, "ARCHIVE archive=%u status=VOICE_REQUESTED t_ms=%llu", story_doc,
                     static_cast<unsigned long long>(now));
            Application::GetInstance().Schedule([] {
                auto& app = Application::GetInstance();
                if (communication_source == CommunicationSource::Story &&
                    app.GetDeviceState() == kDeviceStateIdle)
                    app.InvokeDeviceEvent("打开档案");
            });
        }
    }
    if (wish_ui_request.exchange(false)) {
        communication_source = CommunicationSource::Life;
        active_call.store(98);
        Modal(27);
        Application::GetInstance().Schedule([] {
            if (communication_source == CommunicationSource::Life && active_call.load() == 98 &&
                Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
                Application::GetInstance().InvokeDeviceEvent("我有想法");
        });
    }
    if (social_ui_request.exchange(false) && spring::SocialMatches(s, social) &&
        social.accepted.id) {
        communication_source = CommunicationSource::Life;
        active_call.store(99);
        Modal(27);
        Application::GetInstance().Schedule([] {
            if (communication_source == CommunicationSource::Life &&
                Application::GetInstance().GetDeviceState() == kDeviceStateIdle)
                Application::GetInstance().InvokeDeviceEvent("想说件事");
        });
    }
    if (const auto source = communication_ui_request.exchange(0)) {
        story_playback_receipt.Reset();
        communication_source = source == 3 ? CommunicationSource::Story : CommunicationSource::Life;
        communication_archive.store(255);
        Modal(27);
    }
    if (s.story.archives & ~s.story.archives_read)
        lv_obj_remove_flag(journal_dot, LV_OBJ_FLAG_HIDDEN);
    if (!lv_obj_has_flag(modal, LV_OBJ_FLAG_HIDDEN)) {
        if (modal_kind >= 17)
            RenderStoryModal(s);
        switch (modal_kind) {
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
                RenderEconomyModal(s);
                if (operation_error && now < message_until)
                    spring_ui::TextChanged(modal_hint, operation_error);
                break;
            case 1: {
                const auto& p = selected_slot < 3 ? s.plots[selected_slot] : s.economy.fourth;
                const auto growth = std::min(
                    100u,
                    static_cast<unsigned>(100ull * p.growth_ms /
                                          (s.economy.enabled
                                               ? 600000u
                                               : spring::kGrowMs[static_cast<unsigned>(p.plant)])));
                spring_ui::TextFormat(modal_title, "%s · %u号位",
                                      spring::kPlantNames[static_cast<unsigned>(p.plant)],
                                      selected_slot + 1);
                spring_ui::TextFormat(
                    modal_body, "%s\n成长 %u%%\n拟安排：%s",
                    p.ready ? "成熟可收获"
                    : s.economy.enabled && !(s.economy.paid_mask & (1u << selected_slot))
                        ? "等待本轮投入"
                    : !s.economy.enabled && (!s.water || !s.energy) ? "生长放缓"
                                                                    : "生长中",
                    growth, spring::kPlantNames[selected_plant]);
                const auto phase =
                    p.ready ? 2u
                    : p.growth_ms >= (s.economy.enabled
                                          ? 600000u
                                          : spring::kGrowMs[static_cast<unsigned>(p.plant)]) /
                                         2
                        ? 1u
                        : 0u;
                const auto id = spring_art::PlantId(p.plant, phase);
                const auto* thumbnail = spring_art::Get(id).image;
                if (lv_image_get_src(resource_icon) != thumbnail)
                    lv_image_set_src(resource_icon, thumbnail);
                lv_obj_set_pos(resource_icon, (96 - thumbnail->header.w) / 2,
                               (112 - thumbnail->header.h) / 2);
                lv_obj_set_width(growth_fill, std::max(2u, growth * 318 / 100));
                spring_ui::TextChanged(
                    modal_hint, !s.energy && !(s.economy.paid_mask & (1u << selected_slot)) &&
                                        !p.ready && selected_plant == 0
                                    ? "停产槽：批准后可立即种轨光花"
                                    : "更换后下一轮生效 · 可重复种植");
                if (s.water < 2 && s.energy && !(s.economy.paid_mask & (1u << selected_slot)) &&
                    !p.ready && selected_plant == 1)
                    spring_ui::TextChanged(modal_hint, "缺水停产槽：批准后立即种雨窝苔");
                for (unsigned i = 0; i < 3; ++i)
                    spring_ui::ColorChanged(
                        choices[i], lv_color_hex(selected_plant == i ? 0x8E9A67 : 0x455E50));
                break;
            }
            case 2:
                spring_ui::TextChanged(modal_title, "大树 · 培育与升级");
                spring_ui::TextFormat(
                    modal_body,
                    "阶段：%s\n下一步：%s\n需水%u、藤%u；工作%u秒\n持有水%lu、藤%lu\n%s",
                    s.tree_stage >= 2 ? "繁茂树冠"
                    : s.tree_stage    ? "成树"
                                      : "幼树",
                    s.tree_stage >= 2 ? "全部升级已完成"
                    : s.tree_stage    ? "繁茂树冠，解锁动物住所"
                                      : "成树，休息与鸟来访",
                    s.tree_stage >= 2 ? 0 : spring::TreeWater(s.tree_stage),
                    s.tree_stage >= 2 ? 0 : spring::TreeFiber(s.tree_stage),
                    s.tree_stage >= 2 ? 0 : spring::TreeDuration(s.tree_stage) / 1000,
                    (unsigned long)s.water, (unsigned long)s.fiber,
                    s.tree_work_ms ? (!s.energy ? "缺电暂停，进度保留" : "正在培育，请稍候")
                                   : "确认方案后才投入");
                spring_ui::TextChanged(modal_hint, "投入只扣一次，建设进度可保存");
                if (s.tree_stage >= 2 || s.tree_work_ms ||
                    s.water < spring::TreeWater(s.tree_stage) ||
                    s.fiber < spring::TreeFiber(s.tree_stage) || !s.storage_ok)
                    lv_obj_add_state(primary, LV_STATE_DISABLED);
                else
                    lv_obj_remove_state(primary, LV_STATE_DISABLED);
                break;
            case 3:
                spring_ui::TextChanged(modal_title, "园丁的通讯请求");
                spring_ui::TextChanged(modal_body, spring::Opening(selected_call, s));
                spring_ui::TextChanged(modal_hint, "确认接听后进入语音；离线可读留言");
                if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
                    lv_obj_add_state(primary, LV_STATE_DISABLED);
                else
                    lv_obj_remove_state(primary, LV_STATE_DISABLED);
                break;
            case 4:
                spring_ui::TextChanged(modal_title, s.demo ? "返回正常试玩？" : "进入十倍演示？");
                spring_ui::TextChanged(
                    modal_body,
                    "两个模式分别存档。\n演示不会覆盖正常进度。\n重启默认回到正常模式。");
                spring_ui::TextChanged(modal_hint, "演示仍遵守材料和建设规则");
                break;
            case 5:
                if (s.economy.decision_count) {
                    RenderDecisionLog(s, log_page);
                    break;
                }
                spring_ui::TextChanged(modal_title, "车厢观察日志");
                if (s.event_count) {
                    const auto& e = s.events[s.event_count - 1 - (log_page % s.event_count)];
                    spring_ui::TextFormat(modal_body,
                                          "近期事件 %lu 条\n记录：%s\n大树：%s\n语言阶段：%u",
                                          (unsigned long)s.event_count,
                                          e.type == spring::Event::Harvest       ? "完成收获"
                                          : e.type == spring::Event::TreeGrown   ? "新叶长出"
                                          : e.type == spring::Event::Drought     ? "旅途提醒"
                                          : e.type == spring::Event::Matured     ? "植物成熟"
                                          : e.type == spring::Event::TreeStarted ? "开始培育"
                                          : e.type == spring::Event::Planted     ? "调整下轮种植"
                                          : e.type == spring::Event::Reply       ? "接听通讯"
                                                                                 : "园丁苏醒",
                                          s.tree_stage >= 2 ? "满顶树冠"
                                          : s.tree_stage    ? "成树"
                                          : s.tree_work_ms  ? "培育中"
                                                            : "树苗",
                                          s.story_stage >= 2 ? 1 : 0);
                } else
                    spring_ui::TextChanged(modal_body,
                                           "轻晃设备或点击唤醒，\n与园丁开始这段旅途。\n轻晃协助收"
                                           "获成熟作物，\n雨窝苔由园丁自动收集。");
                spring_ui::TextFormat(
                    modal_hint, "%s · %us · %s\n%s", spring_power::Name(GamePowerMode()),
                    PowerIdleSeconds(),
                    Application::GetInstance().GetDeviceState() == kDeviceStateListening  ? "聆听"
                    : Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking ? "说话"
                    : Application::GetInstance().GetDeviceState() == kDeviceStateConnecting
                        ? "连接"
                        : "待机",
                    s.sound_muted ? "通讯提示已静音" : "通讯提示已开启");
                break;
            case 6: {
                const char* names[] = {"藤纤维", "水", "能源", "食物", "旧零件"};
                const char* descriptions[] = {
                    "缝甲藤收获产出\n用于培育大树", "雨窝苔成熟后自动收集\n用于生产与日常照料",
                    "轨光花收获产出\n维持园丁工作节奏", "旅粮豆收获产出\n用于救助和照料动物",
                    "旅途发现、工作台拆解\n用于关键设施建设"};
                const spring_art::Id icons[] = {
                    spring_art::Id::Icon_fiber, spring_art::Id::Icon_water,
                    spring_art::Id::Icon_energy, spring_art::Id::Icon_food,
                    spring_art::Id::Icon_parts};
                spring_ui::TextChanged(modal_title, names[selected_resource]);
                spring_ui::TextFormat(
                    modal_body, "%lu\n%s",
                    static_cast<unsigned long>(resource_amounts[selected_resource]),
                    descriptions[selected_resource]);
                const auto* icon = spring_art::Get(icons[selected_resource]).image;
                if (lv_image_get_src(resource_icon) != icon)
                    lv_image_set_src(resource_icon, icon);
                break;
            }
        }
    }
}
}  // namespace
bool SetGameTextFont(std::shared_ptr<LvglFont> font) {
    if (!font || !spring_ui::HasGameChinese(font->font()))
        return false;
    local_game_font = std::move(font);
    return true;
}
std::shared_ptr<LvglFont> GetGameTextFont() { return local_game_font; }
void RecordTouchPoll(int64_t started_us, bool pressed) {
    static int64_t last_us = 0, logged_us = 0, max_gap = 0, max_read = 0;
    static unsigned samples = 0, down = 0;
    if (last_us)
        max_gap = std::max(max_gap, started_us - last_us);
    last_us = started_us;
    max_read = std::max<int64_t>(max_read, esp_timer_get_time() - started_us);
    ++samples;
    if (pressed)
        ++down;
    if (started_us - logged_us >= 5000000) {
        ESP_LOGI(kTag, "TOUCH_PERF polls=%u down=%u gap_ms=%lld read_us=%lld", samples, down,
                 max_gap / 1000, max_read);
        logged_us = started_us;
        samples = down = 0;
        max_gap = max_read = 0;
    }
}
bool QueueLifeInvitation(unsigned intent, unsigned target, uint32_t sequence) {
    return Send(Action::InviteLife, intent, target, sequence, spring::MemoryActor::VoiceRequest);
}
bool QueueResumeWork(uint32_t sequence) {
    return Send(Action::ResumeWork, 0, 0, sequence, spring::MemoryActor::VoiceRequest);
}
uint32_t QueuePlantWork(unsigned kind, unsigned slot, uint32_t cycle, bool demo, uint32_t world_hi,
                        uint32_t world_lo) {
    std::lock_guard<std::mutex> guard(plant_status_mutex);
    for (const auto& a : plant_statuses) {
        if (!spring::PlantActionActive(a))
            continue;
        return a.kind == kind && a.slot == slot && a.cycle == cycle ? a.id : 0;
    }
    if (!commands)
        return 0;
    if (!plant_next_id)
        plant_next_id = (esp_random() & 0x3fffffffu) + 1;
    if (plant_next_id >= INT32_MAX)
        return 0;
    const auto id = ++plant_next_id;
    Command c{Action::PlantWork, kind, slot, cycle, spring::MemoryActor::VoiceRequest};
    c.token = id;
    c.demo = demo;
    c.world_hi = world_hi;
    c.world_lo = world_lo;
    if (xQueueSend(commands, &c, 0) != pdTRUE)
        return 0;
    auto& a = plant_statuses[id % plant_statuses.size()];
    a = {id, kind, slot, cycle, spring::PlantActionStatus::Queued};
    return id;
}
spring::PlantAction GetPlantWorkStatus(uint32_t id) {
    std::lock_guard<std::mutex> guard(plant_status_mutex);
    for (const auto& a : plant_statuses)
        if (a.id == id && id)
            return a;
    spring::PlantAction unknown;
    unknown.id = id;
    return unknown;
}
bool QueueActivityCancellation(uint32_t plant_id, uint32_t life_sequence,
                               const spring::Snapshot& expected) {
    if (!commands || !expected.memory)
        return false;
    Command c{Action::CancelActivity, plant_id, life_sequence, 0,
              spring::MemoryActor::VoiceRequest};
    c.demo = expected.demo;
    c.world_hi = expected.memory->world_hi;
    c.world_lo = expected.memory->world_lo;
    return xQueueSend(commands, &c, 0) == pdTRUE;
}
bool QueueWishResponse(uint32_t token, unsigned choice, bool voice_request) {
    return Send(Action::WishReply, token, choice, 0,
                voice_request ? spring::MemoryActor::VoiceRequest : spring::MemoryActor::Player);
}
uint32_t QueueVoiceWave() {
    const auto prior = wave_status.load();
    if (prior == 1 || prior == 2)
        return 0;
    const uint32_t id = wave_next_id.fetch_add(1) + 1;
    wave_request_id.store(id);
    wave_status.store(1);
    if (!Send(Action::Wave, id, 0, 0, spring::MemoryActor::VoiceRequest)) {
        wave_status.store(4);
        return 0;
    }
    ESP_LOGI(kTag, "VOICE_ACTION id=%lu kind=WAVE_HAND status=QUEUED t_ms=%llu",
             static_cast<unsigned long>(id),
             static_cast<unsigned long long>(esp_timer_get_time() / 1000));
    return id;
}
const char* VoiceWaveStatus(uint32_t request_id) {
    if (request_id != wave_request_id.load())
        return "UNKNOWN";
    switch (wave_status.load()) {
        case 1:
            return "QUEUED";
        case 2:
            return "RUNNING";
        case 3:
            return "COMPLETED";
        case 4:
            return "REJECTED";
        default:
            return "UNKNOWN";
    }
}
bool QueueEconomyProposal(unsigned kind, unsigned target, unsigned value) {
    return Send(Action::Propose, kind, target, value, spring::MemoryActor::VoiceRequest);
}
void GetWorldSnapshot(spring::Snapshot& out) {
    std::lock_guard<std::mutex> guard(world_mutex);
    world.Get(out);
    out.motion_ok = motion_ok.load();
}
spring::Snapshot GetWorldSnapshot() {
    spring::Snapshot out;
    GetWorldSnapshot(out);
    return out;
}
void MotionSample(float angle, bool stable) {
    tilt_angle.store(angle);
    tilt_stable.store(stable);
    tilt_at.store(esp_timer_get_time() / 1000);
}
bool QueueStoryAction(unsigned verb, unsigned target, unsigned value, unsigned token,
                      const char* text, bool voice_request) {
    if (verb > static_cast<unsigned>(spring::StoryVerb::EndCommunication) || !text ||
        std::strlen(text) > 48)
        return false;
    Command c{Action::Story, verb, target, value,
              voice_request ? spring::MemoryActor::VoiceRequest : spring::MemoryActor::Player};
    c.token = token;
    c.context_epoch = roadshow_input_epoch.load();
    std::memcpy(c.text.data(), text, std::strlen(text));
    if (verb != static_cast<unsigned>(spring::StoryVerb::Announce) &&
        verb != static_cast<unsigned>(spring::StoryVerb::ShowLife) &&
        verb != static_cast<unsigned>(spring::StoryVerb::Presented))
        PowerActivity("story_input");
    return commands && xQueueSend(commands, &c, 0) == pdTRUE;
}
bool QueueVoiceApproval(unsigned token) {
    return Send(Action::ConfirmProposal, token, 0, 0, spring::MemoryActor::VoiceRequest);
}
unsigned ActiveCommunication() { return active_call.load(); }
std::string CharacterContextJson(const spring::Snapshot& s) {
    std::lock_guard<std::mutex> guard(social_mutex);
    return spring::AgencyObservationJson(
        s, character_public, agency_public,
        communication_source == CommunicationSource::Life && active_call.load() == 98);
}
std::string CommunicationContextJson(const spring::Snapshot& s) {
    return CommunicationJson(s, true);
}
void SetMotionAvailable(bool available) { motion_ok.store(available); }
void MotionShakeDetected() {
    if (!PowerMotionAllowed())
        return;
    Send(Action::Shake);
    tilt_stable.store(false);
}
void PickupWakeFeedback() { picked_up.store(true); }
void GardenerTranscript(const char* role, const char* text) {
    if (!role || !text || !text[0])
        return;
    if (std::strcmp(role, "user") == 0) {
        ++user_turn;
        voice_wait_at.store(esp_timer_get_time() / 1000);
    } else if (std::strcmp(role, "assistant") == 0) {
        voice_wait_at.store(0);
        if (active_call.load() == 6) {
            const auto snapshot = GetWorldSnapshot();
            if (communication_source == CommunicationSource::Story &&
                snapshot.story.active == communication_story_node.load())
                story_response_node.store(snapshot.story.active);
            // The LVGL observer records presentation after actual playback drains,
            // not on a fragile exact subtitle match or merely opening the card.
        }
    }
}
void VoiceInteractionCompleted() { /* Conversations never mint resources or advance story. */ }
void RecordMonitorQuery(unsigned kind) {
    if (kind < monitor_queries.size())
        ++monitor_queries[kind];
}
void StartDay1Probe(lv_obj_t* parent) {
    commands = xQueueCreate(8, sizeof(Command));
    if (!commands) {
        ESP_LOGE(kTag, "Command queue allocation failed");
        return;
    }
    RefreshFont();
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    stage = Box(parent, 0, 0, 480, 480, 0x263C32);
    spring_art::Create(stage);
    ESP_LOGI(kTag, "ART_READY flash_bytes=%u; native RGB565/I8, single active robot frame",
             static_cast<unsigned>(spring_art::kFlashBytes));
    auto* scene = spring_art::World();
    hud = Box(lv_layer_top(), 0, 0, 480, 480, 0, 0);
    lv_obj_set_style_bg_opa(hud, LV_OPA_TRANSP, 0);
    for (unsigned i = 0; i < 4; ++i) {
        auto* hit = spring_ui::Target(
            scene, {static_cast<int>(i == 3 ? 446 : 130 + 104 * i) - 49, 335, 98, 113});
        plot_hits[i] = hit;
        if (i == 3)
            lv_obj_add_flag(hit, LV_OBJ_FLAG_HIDDEN);
        touch_checks.push_back({spring::kPlantNames[i], hit});
        lv_obj_add_event_cb(
            hit,
            [](lv_event_t* e) {
                ShowPlant(
                    static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
            },
            LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    }
    auto* tree_hit = spring_ui::Target(scene, spring_ui::kWorldTree);
    touch_checks.push_back({"大树", tree_hit});
    lv_obj_add_event_cb(
        tree_hit,
        [](lv_event_t*) {
            Modal(2);
            lv_obj_remove_flag(primary, LV_OBJ_FLAG_HIDDEN);
        },
        LV_EVENT_CLICKED, nullptr);
    // Above the broad tree/plot targets so facilities remain independently tappable.
    spring_ui::CreateFacilityTargets(scene, facility_hits, [](lv_event_t* e) {
        const unsigned i =
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
        EconomyModal(15);
        economy_page = i;
    });
    for (unsigned i = 0; i < 9; ++i)
        if (facility_hits[i])
            touch_checks.push_back({spring::kFacilities[i == 8 ? 7 : i].name, facility_hits[i]});
    const spring_art::Id icons[] = {spring_art::Id::Icon_fiber, spring_art::Id::Icon_water,
                                    spring_art::Id::Icon_energy, spring_art::Id::Icon_food,
                                    spring_art::Id::Icon_parts};
    for (unsigned i = 0; i < 5; ++i) {
        const auto r = spring_ui::kResources[i];
        auto* pill = spring_ui::Target(hud, r);
        lv_obj_set_style_bg_color(pill, lv_color_hex(0x2F392C), 0);
        lv_obj_set_style_bg_opa(pill, LV_OPA_90, 0);
        lv_obj_set_style_radius(pill, 22, 0);
        lv_obj_set_style_border_width(pill, 1, 0);
        lv_obj_set_style_border_color(pill, lv_color_hex(0xB9B898), 0);
        auto* icon = spring_art::Image(pill, icons[i], 3, 9);
        (void)icon;
        resource_values[i] = Text(pill, 29, 9, 36, "0");
        lv_obj_add_event_cb(
            pill,
            [](lv_event_t* e) {
                selected_resource =
                    static_cast<unsigned>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
                Modal(6);
            },
            LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        touch_checks.push_back({"资源", pill});
    }
    auto* journal = spring_ui::Target(hud, spring_ui::kJournal);
    lv_obj_set_style_bg_color(journal, lv_color_hex(0x2F392C), 0);
    lv_obj_set_style_bg_opa(journal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(journal, 16, 0);
    spring_art::Image(journal, spring_art::Id::Icon_journal, 8, 8);
    lv_obj_add_event_cb(journal, [](lv_event_t*) { StoryModal(17); }, LV_EVENT_CLICKED, nullptr);
    journal_dot = Box(journal, 41, 3, 8, 8, 0xF28A4D, 4);
    lv_obj_add_flag(journal_dot, LV_OBJ_FLAG_HIDDEN);
    touch_checks.push_back({"日志", journal});
    call = spring_ui::Target(hud, spring_ui::kCommunication);
    lv_obj_set_style_bg_color(call, lv_color_hex(0x354A35), 0);
    lv_obj_set_style_bg_opa(call, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(call, 36, 0);
    lv_obj_set_style_border_width(call, 2, 0);
    spring_ui::RadioGlyph(call);
    lv_obj_add_event_cb(call, OnCall, LV_EVENT_CLICKED, nullptr);
    call_dot = Box(call, 57, 5, 9, 9, 0xF7D16A, 5);
    lv_obj_add_flag(call_dot, LV_OBJ_FLAG_HIDDEN);
    touch_checks.push_back({"通讯", call});
    story_robot_hit = spring_ui::Target(hud, {0, 0, 64, 76});
    lv_obj_set_style_bg_opa(story_robot_hit, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(
        story_robot_hit,
        [](lv_event_t*) {
            QueueStoryAction(static_cast<unsigned>(spring::StoryVerb::Observe), 255, 0, 0, "",
                             false);
        },
        LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(story_robot_hit, LV_OBJ_FLAG_HIDDEN);
    social_bubble = Box(hud, 220, 180, 64, 48, 0xF2ECD8, 18);
    spring_ui::ConfigureInvitationBubble(social_bubble);
    lv_obj_set_style_border_width(social_bubble, 2, 0);
    lv_obj_set_style_border_color(social_bubble, lv_color_hex(0x5C7957), 0);
    Text(social_bubble, 20, 10, 32, "…", 0x354A35);
    lv_obj_add_event_cb(
        social_bubble,
        [](lv_event_t*) {
            const auto snapshot =
                std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
            const bool opened = OpenCommunicationInvitation(*snapshot, false);
            ESP_LOGI(kTag, "COMM_ENTRY origin=BUBBLE opened=%u", opened);
        },
        LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(social_bubble, LV_OBJ_FLAG_HIDDEN);
    touch_checks.push_back({"主动交流泡泡", social_bubble});
    caption = Text(hud, 12, 448, 456, "");
    lv_obj_add_flag(caption, LV_OBJ_FLAG_HIDDEN);
    notice_text = Text(hud, 12, 448, 456, "", 0xFFF4C6);
    lv_obj_set_style_bg_color(notice_text, lv_color_hex(0x18362B), 0);
    lv_obj_set_style_bg_opa(notice_text, LV_OPA_80, 0);
    lv_obj_add_flag(notice_text, LV_OBJ_FLAG_HIDDEN);
    modal_backdrop = spring_ui::Target(lv_layer_top(), {0, 0, 480, 480});
    lv_obj_set_style_bg_color(modal_backdrop, lv_color_hex(0x101A18), 0);
    lv_obj_set_style_bg_opa(modal_backdrop, LV_OPA_TRANSP, 0);
    modal = Box(modal_backdrop, 53, 78, 374, 333, 0xF2ECD8, 20);
    lv_obj_add_flag(modal, LV_OBJ_FLAG_CLICKABLE);
    modal_title = Text(modal, 20, 16, 330, "园丁的通讯请求", 0x293A28);
    modal_body = Text(modal, 20, 57, 328, "", 0x3A4933);
    archive_scroll = Box(modal, 20, 52, 424, 128, 0xF2ECD8);
    lv_obj_add_flag(archive_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(archive_scroll, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scroll_dir(archive_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(archive_scroll, LV_SCROLLBAR_MODE_AUTO);
    archive_content = Box(archive_scroll, 0, 0, 422, 1, 0xF2ECD8);
    archive_text = Text(archive_content, 2, 2, 398, "", 0x3A4933);
    small_labels.push_back(archive_text);
    LabelScale(archive_text);
    lv_obj_add_flag(archive_scroll, LV_OBJ_FLAG_HIDDEN);
    modal_hint = Text(modal, 20, 174, 328, "", 0x667451);
    small_labels.push_back(modal_hint);
    LabelScale(modal_hint);
    plant_tile = Box(modal, 16, 56, 96, 112, 0xDEE4C9, 12);
    growth_track = Box(modal, 126, 154, 218, 4, 0xCDD1B2, 2);
    growth_fill = Box(growth_track, 0, 0, 2, 4, 0x749A4A, 2);
    lv_obj_add_flag(plant_tile, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(growth_track, LV_OBJ_FLAG_HIDDEN);
    resource_icon = spring_art::Image(modal, spring_art::Id::Icon_water, 20, 62);
    economy_icon = spring_art::Image(modal, spring_art::Id::Icon_food, 300, 8);
    lv_obj_add_flag(economy_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(resource_icon, LV_OBJ_FLAG_HIDDEN);
    for (unsigned i = 0; i < 3; ++i) {
        choices[i] = Button(
            modal, 14 + i * 117, 211, 111, 44, spring::kPlantNames[i],
            [](lv_event_t* e) {
                const auto action = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
                if (modal_kind == 5) {
                    const auto snapshot =
                        std::unique_ptr<spring::Snapshot>(new spring::Snapshot(GetWorldSnapshot()));
                    const auto& log = *snapshot;
                    const auto count =
                        log.economy.decision_count ? log.economy.decision_count : log.event_count;
                    if (action == 2)
                        Send(Action::Mute);
                    else if (count)
                        log_page = (log_page + (action ? 1 : count - 1)) % count;
                } else if (modal_kind >= 17)
                    StoryChoice(action);
                else if (modal_kind >= 7)
                    EconomyChoice(action);
                else if (modal_kind == 1) {
                    if (action == 2)
                        EconomyModal(7);
                    else
                        selected_plant = (selected_plant + (action ? 1 : 3)) % 4;
                }
            },
            i);
        choice_labels[i] = text_nodes.back();
    }
    primary = Button(modal, 17, 272, 150, 46, "确认", Confirm);
    back_button = Button(modal, 203, 272, 150, 46, "返回", HideModal);
    lv_obj_add_event_cb(
        modal_backdrop,
        [](lv_event_t* e) {
            if (modal_kind == 6 && lv_event_get_target(e) == modal_backdrop)
                HideModal(nullptr);
        },
        LV_EVENT_CLICKED, nullptr);
    spring_ui::DialogVisible(modal_backdrop, modal, false);
    TaskHandle_t task = nullptr;
    if (xTaskCreate(Worker, "spring_world", 24576, nullptr, 1, &task) != pdPASS) {
        ESP_LOGE(kTag, "Worker allocation failed");
        return;
    }
    if (xTaskCreate(MonitorWorker, "spring_monitor", 12288, nullptr, 1, nullptr) != pdPASS)
        ESP_LOGE(kTag, "Read-only monitor allocation failed");
    auto* display = lv_display_get_default();
    lv_display_add_event_cb(display, RefreshEvent, LV_EVENT_REFR_START, nullptr);
    lv_display_add_event_cb(display, RefreshEvent, LV_EVENT_REFR_READY, nullptr);
    lv_timer_create(Render, 50, nullptr);
    ESP_LOGI(kTag, "Spring MVP started; old desk_gap_v1 retained; input/voice/drivers reused");
}
}  // namespace desk_gap
