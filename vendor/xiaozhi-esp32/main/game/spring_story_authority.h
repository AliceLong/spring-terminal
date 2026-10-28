#pragma once
#include <cstdint>
namespace spring {
inline bool FreshVoiceConfirmation(uint32_t current_turn, uint32_t offer_turn) {
    return current_turn > offer_turn;
}
}  // namespace spring
