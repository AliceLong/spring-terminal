#pragma once
#include <algorithm>
#include "spring_story_types.h"

namespace spring {
// P0 ships levels 0 and 1. Later language does not silently enable unbuilt autonomy.
constexpr unsigned kImplementedAgencyLevel = 1;
inline unsigned AgencyLevel(const StoryState& story) {
    return std::min<unsigned>(story.enabled ? story.language : 0, kImplementedAgencyLevel);
}
inline bool PersonalLife(unsigned intent, unsigned target) {
    return intent == 1 || intent == 3 || (intent == 2 && target >= 4);
}
inline unsigned AgencyMode(const StoryState& story, unsigned intent, unsigned target) {
    if (!story.enabled || !PersonalLife(intent, target))
        return 2;               // Routine maintenance.
    return AgencyLevel(story);  // 0 explicit instruction; 1 propose and wait; 2 act.
}
}  // namespace spring
