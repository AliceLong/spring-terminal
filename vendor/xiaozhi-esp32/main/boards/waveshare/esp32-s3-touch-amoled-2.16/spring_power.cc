#include "spring_power.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>
#include "application.h"
#include "board.h"
#include "display/display.h"
namespace desk_gap {
namespace {
std::mutex mutex;
spring_power::State power;
std::function<void(spring_power::Mode)> apply_mode;
std::atomic<spring_power::Mode> published{spring_power::Mode::Play};
std::atomic<unsigned> idle_seconds{0};
std::atomic<bool> apply_pending{false};
bool configured = false, first_apply = true;
const char* reason = "boot";
int previous_audio = -1;
uint64_t voice_progress = 0, setup_started = 0;
uint32_t previous_playback = 0;
uint64_t Now() { return esp_timer_get_time() / 1000; }
void Publish(uint64_t now) {
    idle_seconds.store((now - power.last_activity) / 1000);
    const bool changed = published.exchange(power.mode) != power.mode;
    if (changed || first_apply) {
        ESP_LOGI("SpringPower", "mode=%s idle_s=%u reason=%s", spring_power::Name(power.mode),
                 idle_seconds.load(), reason);
        if (configured && !apply_pending.exchange(true)) {
            first_apply = false;
            Application::GetInstance().Schedule([] {
                std::function<void(spring_power::Mode)> apply;
                spring_power::Mode mode;
                {
                    std::lock_guard<std::mutex> guard(mutex);
                    apply = apply_mode;
                    mode = power.mode;
                    apply_pending = false;
                }
                if (apply)
                    apply(mode);
            });
        }
    }
}
}  // namespace
void ConfigureGamePower(std::function<void(spring_power::Mode)> apply) {
    std::lock_guard<std::mutex> guard(mutex);
    apply_mode = std::move(apply);
    configured = true;
    first_apply = true;
    power.Activity(Now());
    Publish(Now());
}
void PowerActivity(const char* source) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto now = Now();
    reason = source;
    power.Activity(now);
    Publish(now);
}
bool PowerTouch(bool down) {
    std::lock_guard<std::mutex> guard(mutex);
    const auto now = Now();
    reason = "touch";
    const bool consume = power.Touch(now, down);
    Publish(now);
    return consume;
}
void PowerVoiceProgress() {
    std::lock_guard<std::mutex> guard(mutex);
    voice_progress = Now();
}
spring_power::Mode GamePowerMode() { return published.load(); }
unsigned PowerIdleSeconds() { return idle_seconds.load(); }
bool PowerMotionAllowed() {
    std::lock_guard<std::mutex> guard(mutex);
    return power.mode != spring_power::Mode::Sleep && Now() >= power.motion_block_until;
}
void PollGamePower() {
    auto& app = Application::GetInstance();
    const auto audio = app.GetDeviceState();
    const bool voice = audio == kDeviceStateConnecting || audio == kDeviceStateListening ||
                       audio == kDeviceStateSpeaking;
    const bool setup = audio == kDeviceStateUnknown || audio == kDeviceStateStarting ||
                       audio == kDeviceStateActivating || audio == kDeviceStateUpgrading ||
                       audio == kDeviceStateWifiConfiguring || audio == kDeviceStateAudioTesting;
    bool timeout = false;
    {
        std::lock_guard<std::mutex> guard(mutex);
        const auto now = Now();
        if (audio != previous_audio)
            setup_started = now;
        const bool setup_grace = setup && now - setup_started < 120000;
        if (voice) {
            const auto playback = app.GetAudioService().PlaybackCount();
            if (audio != previous_audio || playback != previous_playback ||
                (audio == kDeviceStateListening && app.GetAudioService().IsVoiceDetected()))
                voice_progress = now;
            previous_playback = playback;
            if (now - voice_progress >= 45000) {
                timeout = true;
                voice_progress = now;
            }
            reason = "voice";
        } else if (previous_audio == kDeviceStateConnecting ||
                   previous_audio == kDeviceStateListening ||
                   previous_audio == kDeviceStateSpeaking) {
            power.Activity(now);
            reason = "voice_end";
        }
        previous_audio = audio;
        power.Tick(now, setup_grace || voice);
        if (!voice && !setup)
            reason = "idle_timer";
        Publish(now);
    }
    if (timeout) {
        ESP_LOGW("SpringPower",
                 "Voice inactive for45s: closing stalled turn through application APIs");
        app.Schedule([] {
            auto& app = Application::GetInstance();
            const auto state = app.GetDeviceState();
            if (state == kDeviceStateListening) {
                app.ToggleChatState();  // Existing close-channel path.
            } else if (state == kDeviceStateSpeaking) {
                app.AbortSpeaking(kAbortReasonNone);
                if (app.SetDeviceState(kDeviceStateListening))
                    app.ToggleChatState();
            } else if (state == kDeviceStateConnecting) {
                app.SetDeviceState(kDeviceStateIdle);
            }
            Board::GetInstance().GetDisplay()->SetChatMessage("system", "语音等待超时，请重试");
        });
    }
}
}  // namespace desk_gap
