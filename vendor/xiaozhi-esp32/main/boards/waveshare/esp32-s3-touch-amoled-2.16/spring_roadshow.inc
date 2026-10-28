// Included in the board world worker. All NVS and world changes stay on that task.
struct RoadshowReceipt {
    spring_show::Packet packet;
    const char* status = "IDLE";
    const char* reason = "NONE";
    uint64_t deadline = 0;
    uint32_t formal_hi = 0, formal_lo = 0;
    uint32_t formal_checksum = 0;
    bool formal_return_verified = false;
};
std::mutex roadshow_mutex;
RoadshowReceipt roadshow_receipt;
struct FrozenFormal {
    spring::Core game;
    spring::SocialState social;
    spring::CharacterState character;
    spring::AgencyState agency;
    uint64_t next_wish_remaining = 0, wish_remaining = 0;
};
std::unique_ptr<FrozenFormal> frozen_formal;
void RecoverFormalRoadshow(spring::Core& formal) {
    nvs_handle_t recover = 0;
    if (!formal.s.memory ||
        nvs_open_from_partition("game_nvs", "roadshow_v1", NVS_READWRITE, &recover) != ESP_OK)
        return;
    std::array<uint32_t, 3> marker{};
    size_t size = sizeof(marker);
    if (nvs_get_blob(recover, "formal_pause", marker.data(), &size) == ESP_OK &&
        size == sizeof(marker) && marker[0] == 1 && marker[1] == formal.s.memory->world_hi &&
        marker[2] == formal.s.memory->world_lo) {
        // A power interruption during a roadshow must not charge its elapsed time to formal play.
        formal.clock_ok = true;
        marker[0] = 0;
        const bool cleared =
            nvs_set_blob(recover, "formal_pause", marker.data(), sizeof(marker)) == ESP_OK &&
            nvs_commit(recover) == ESP_OK;
        ESP_LOGI(kTag, "ROADSHOW_BOOT formal_restored=1 pause_marker_cleared=%d", cleared);
        (void)cleared;
    }
    nvs_close(recover);
}
void SubmitRoadshow(const spring_show::Packet& p, const spring::Core& game, uint64_t now) {
    std::lock_guard<std::mutex> lock(roadshow_mutex);
    if (roadshow_receipt.packet.id == p.id && roadshow_receipt.packet.boot == p.boot)
        return;  // Duplicate delivery cannot reset a fixture or replace its receipt.
    if (roadshow_pending.load())
        return;  // One bounded request, no queue of future scene changes.
    roadshow_receipt.packet = p;
    roadshow_receipt.status = "REJECTED";
    roadshow_receipt.reason = "STALE_WORLD";
    if (!spring_show::Fresh(p, game, mind_boot.load()))
        return;
    if (!game.storage_ok || (!game.roadshow && game.demo)) {
        roadshow_receipt.reason = "FORMAL_MODE_REQUIRED";
        return;
    }
    roadshow_receipt.status = "WAITING";
    roadshow_receipt.reason = "SAFE_POINT";
    roadshow_receipt.deadline = now + 60000;
    roadshow_pending.store(true);
}
std::string RoadshowJson(const spring::Snapshot& s) {
    std::lock_guard<std::mutex> lock(roadshow_mutex);
    const auto& r = roadshow_receipt;
    std::ostringstream o;
    o << "{\"available\":true,\"protocol\":1,\"scene_count\":" << spring_show::kSceneCount
      << ",\"active\":" << (s.roadshow ? "true" : "false") << ",\"scene\":" << s.roadshow_scene
      << ",\"epoch\":" << s.roadshow_epoch << ",\"label\":\""
      << spring_show::SceneName(s.roadshow_scene) << "\",\"request_id\":" << r.packet.id
      << ",\"target\":" << r.packet.scene << ",\"status\":\"" << r.status << "\",\"reason\":\""
      << r.reason << "\",\"formal_hi\":" << r.formal_hi << ",\"formal_lo\":" << r.formal_lo
      << ",\"formal_checksum\":" << r.formal_checksum
      << ",\"formal_return_verified\":" << (r.formal_return_verified ? "true" : "false")
      << ",\"fixture\":" << (s.roadshow ? "true" : "false") << '}';
    return o.str();
}
bool ApplyRoadshow(spring::Core& current, nvs_handle_t& handle, nvs_handle_t formal_handle,
                   spring::SocialState& social, spring::CharacterState& character,
                   spring::AgencyState& agency, uint64_t now, bool voice_idle, bool queue_empty,
                   uint64_t* next_wish = nullptr, uint64_t* wish_expires = nullptr) {
    if (!roadshow_pending.load())
        return false;
    std::lock_guard<std::mutex> lock(roadshow_mutex);
    auto& receipt = roadshow_receipt;
    auto fail = [&](const char* reason) {
        PublishSocial(social);
        PublishCharacter(character);
        PublishAgency(agency);
        receipt.status = "REJECTED";
        receipt.reason = reason;
        roadshow_pending.store(false);
        ESP_LOGW(kTag, "ROADSHOW id=%lu status=REJECTED reason=%s",
                 (unsigned long)receipt.packet.id, reason);
        return false;
    };
    if (!spring_show::Fresh(receipt.packet, current, mind_boot.load()))
        return fail("STALE_WORLD");
    if (now >= receipt.deadline)
        return fail("WAIT_TIMEOUT");
    receipt.reason = spring_show::Blocker(current, voice_idle, queue_empty);
    if (std::strcmp(receipt.reason, "NONE"))
        return false;
    if (!current.storage_ok)
        return fail("STORAGE_UNAVAILABLE");
    const auto scene = receipt.packet.scene;
    if (scene == spring_show::kResume && current.roadshow)
        return fail("ALREADY_ROADSHOW");
    if (!scene && !current.roadshow) {
        receipt.status = "APPLIED";
        roadshow_pending.store(false);
        return false;
    }
    if (!scene && !frozen_formal)
        return fail("NO_FORMAL_CHECKPOINT");
    auto next = std::make_unique<spring::Core>();
    spring::SocialState next_social;
    spring::CharacterState next_character;
    spring::AgencyState next_agency;
    nvs_handle_t destination = formal_handle;
    if (scene) {
        if (!roadshow_handle && nvs_open_from_partition("game_nvs", "roadshow_v1", NVS_READWRITE,
                                                        &roadshow_handle) != ESP_OK)
            return fail("ROADSHOW_STORAGE_OPEN");
        destination = roadshow_handle;
        if (scene == spring_show::kResume) {
            std::array<uint32_t, 3> identity{};
            size_t size = sizeof(identity);
            if (nvs_get_blob(destination, "identity", identity.data(), &size) != ESP_OK ||
                size != sizeof(identity) || !LoadGame(destination, true, *next) ||
                !next->s.memory || next->s.memory->world_hi != identity[0] ||
                next->s.memory->world_lo != identity[1] || !identity[2] ||
                identity[2] > spring_show::kSceneCount)
                return fail("NO_ROADSHOW_SAVE");
            next->roadshow = true;
            next->roadshow_scene = identity[2];
            next_social = LoadSocial(destination, next->Get());
            next_character = LoadCharacter(destination, next->Get());
            next_agency = LoadAgency(destination, next->Get());
        } else {
            if (!spring_show::Prepare(*next, scene, esp_random(), esp_random() | 1u))
                return fail("INVALID_SCENE");
            next_social = spring::EmptySocial(next->Get());
            next_character = spring::EmptyCharacter(next->Get());
            next_agency = spring::EmptyAgency(next->Get());
        }
    } else {
        *next = frozen_formal->game;
        next_social = frozen_formal->social;
        next_character = frozen_formal->character;
        next_agency = frozen_formal->agency;
        auto check = next->PackStory();
        if (!check || check->checksum != receipt.formal_checksum)
            return fail("FORMAL_CHECKPOINT_MISMATCH");
        receipt.formal_return_verified = true;
    }
    // Do not settle time spent presenting into either paused save.
    if (utc_ready.load()) {
        next->clock_ok = true;
        next->s.wall_epoch = static_cast<int64_t>(time(nullptr));
        next->s.unsynced_ms = 0;
    }
    next->roadshow_epoch = current.roadshow_epoch + 1;
    // Commit the source before leaving it. Formal and roadshow use different NVS handles.
    if (!Persist(handle, current) || !PersistSocial(handle, current.demo, social) ||
        !PersistCharacter(handle, current.demo, character) ||
        !PersistAgency(handle, current.demo, agency))
        return fail("SOURCE_SAVE_FAILED");
    if (scene) {
        const std::array<uint32_t, 3> identity{next->s.memory->world_hi, next->s.memory->world_lo,
                                               next->roadshow_scene};
        if (!Persist(destination, *next) || !PersistSocial(destination, true, next_social) ||
            !PersistCharacter(destination, true, next_character) ||
            !PersistAgency(destination, true, next_agency) ||
            nvs_set_blob(destination, "identity", identity.data(), sizeof(identity)) != ESP_OK ||
            nvs_commit(destination) != ESP_OK)
            return fail("ROADSHOW_SAVE_FAILED");
    } else if (!Persist(formal_handle, *next)) {
        return fail("FORMAL_SAVE_FAILED");
    }
    const auto* formal_memory =
        current.roadshow ? frozen_formal->game.s.memory.get() : current.s.memory.get();
    const std::array<uint32_t, 3> marker{scene ? 1u : 0u, formal_memory->world_hi,
                                         formal_memory->world_lo};
    if (nvs_set_blob(roadshow_handle, "formal_pause", marker.data(), sizeof(marker)) != ESP_OK ||
        nvs_commit(roadshow_handle) != ESP_OK)
        return fail("PAUSE_MARKER_FAILED");
    if (!current.roadshow && scene) {
        frozen_formal =
            std::make_unique<FrozenFormal>(FrozenFormal{current, social, character, agency});
        receipt.formal_hi = current.s.memory->world_hi;
        receipt.formal_lo = current.s.memory->world_lo;
        receipt.formal_checksum = current.PackStory()->checksum;
        receipt.formal_return_verified = false;
        frozen_formal->next_wish_remaining = next_wish && *next_wish > now ? *next_wish - now : 0;
        frozen_formal->wish_remaining =
            wish_expires && *wish_expires > now ? *wish_expires - now : 0;
    }
    current = std::move(*next);
    roadshow_applied_epoch.store(current.roadshow_epoch);
    handle = destination;
    social = next_social;
    character = next_character;
    agency = next_agency;
    if (next_wish)
        *next_wish = now + (scene ? 30000 : frozen_formal->next_wish_remaining);
    if (wish_expires)
        *wish_expires =
            scene ? (spring::WishPending(agency) ? now + 600000 : 0)
                  : (frozen_formal->wish_remaining ? now + frozen_formal->wish_remaining : 0);
    if (!scene)
        frozen_formal.reset();
    PublishSocial(social);
    PublishCharacter(character);
    PublishAgency(agency);
    mind_offer.fetch_add(1);  // Reject any in-flight model reply from the previous context.
    mind_deadline.store(0);
    mind_reply_deadline.store(0);
    mind_mask.store(0);
    communication_ui_request.store(0);
    story_ui_request.store(0);
    active_call.store(99);
    opening_request.store(99);
    proposal_request.store(0);
    wish_ui_request.store(false);
    wave_status.store(0);
    {
        std::lock_guard<std::mutex> lock(plant_status_mutex);
        plant_statuses = {};
    }
    receipt.status = "APPLIED";
    receipt.reason = "NONE";
    roadshow_pending.store(false);
    PowerActivity("roadshow_switch");
    ESP_LOGI(kTag, "ROADSHOW id=%lu status=APPLIED scene=%lu epoch=%lu formal=%08lx%08lx",
             (unsigned long)receipt.packet.id, (unsigned long)current.roadshow_scene,
             (unsigned long)current.roadshow_epoch, (unsigned long)receipt.formal_hi,
             (unsigned long)receipt.formal_lo);
    return true;
}
