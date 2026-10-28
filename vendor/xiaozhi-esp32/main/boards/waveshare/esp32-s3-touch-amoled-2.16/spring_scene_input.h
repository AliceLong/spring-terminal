#pragma once
#include <atomic>
#include <cstdint>
namespace spring_ui {
// UI-thread only. Discard old controls/touch edges before publishing the new
// input epoch. The worker still rejects commands from older worlds.
template <class ClearOldUi>
bool SyncSceneInput(uint32_t epoch, uint32_t& displayed, std::atomic<uint32_t>& input,
                    ClearOldUi clear_old_ui) {
    if (displayed == epoch)
        return false;
    clear_old_ui();
    displayed = epoch;
    input.store(epoch);
    return true;
}
}  // namespace spring_ui
