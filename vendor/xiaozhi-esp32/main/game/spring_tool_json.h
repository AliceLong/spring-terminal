#pragma once
#include "spring_json.h"
namespace spring {
// Includes escaped tool text, JSON-RPC/MCP wrappers, a signed request id and
// a 64-byte session id. Keep another 1KiB below the broker's 8KiB ceiling.
constexpr size_t kGameToolReplyBudget = 7168;
inline size_t GameToolReplyBytes(const std::string& text) {
    return (std::string("{\"session_id\":\"") + std::string(64, 's') +
            "\",\"type\":\"mcp\",\"payload\":{\"jsonrpc\":\"2.0\",\"id\":-2147483648,"
            "\"result\":{\"content\":[{\"type\":\"text\",\"text\":" +
            StoryQuote(text.c_str()) + "}],\"isError\":false}}}")
        .size();
}
inline std::string ToolWorldJson(const Snapshot& s, unsigned active_call = 99,
                                 const std::string& communication = "") {
    auto json = WorldJson(s, active_call, true);
    if (!communication.empty()) {
        json.pop_back();
        json += ",\"communication\":" + communication + "}";
    }
    return json;
}
inline std::string ToolMemoryJson(const Snapshot& s, unsigned category, uint32_t chain,
                                  uint32_t after, unsigned limit, uint32_t before,
                                  unsigned fact_offset, unsigned fact_limit) {
    // Real records are paged, never byte-truncated or silently discarded.
    limit = std::max(1u, std::min(limit, 24u));
    fact_limit = std::max(1u, std::min(fact_limit, 8u));
    while (true) {
        auto json =
            MemoryJson(s, category, chain, after, limit, false, before, fact_offset, fact_limit);
        if (GameToolReplyBytes(json) <= kGameToolReplyBudget)
            return json;
        if (limit > 1)
            --limit;
        else if (fact_limit > 1)
            --fact_limit;
        else
            return "{\"error\":\"reply_budget_exceeded\",\"retry_with_filters\":true}";
    }
}
// Cursors bind an offset to the same world, mode and story revision. They do
// not change the save format and cannot silently cross a rolling-buffer update.
inline std::string StoryRecordCursor(const Snapshot& s, unsigned offset) {
    const auto world = s.memory ? MemoryWorldId(*s.memory, s.demo)
                                : (s.demo ? "demo-untracked" : "normal-untracked");
    return "s1:" + world + ":" + std::to_string(s.story.revision) + ":" +
           std::to_string(s.story.count) + ":" + std::to_string(offset);
}
inline std::string StoryRecordPageError(const char* reason) {
    return "{\"error\":" + StoryQuote(reason) +
           ",\"restart_required\":true,\"records\":[],\"retry\":{\"record_offset\":0,"
           "\"record_cursor\":\"\"}}";
}
inline std::string ToolStoryJson(const Snapshot& s, unsigned node, unsigned offset, unsigned limit,
                                 const std::string& cursor = "") {
    if (cursor.empty()) {
        if (offset)
            return StoryRecordPageError("story_record_cursor_required");
    } else {
        unsigned position = 0;
        for (unsigned i = 1; i < s.story.count; ++i)
            if (cursor == StoryRecordCursor(s, i)) {
                position = i;
                break;
            }
        if (!position || (offset && offset != position))
            return StoryRecordPageError("story_record_cursor_stale_or_invalid");
        offset = position;
    }
    limit = std::max(1u, std::min(limit, 16u));
    while (true) {
        const auto end = std::min<unsigned>(s.story.count, offset + limit);
        const auto next = end < s.story.count ? StoryRecordCursor(s, end) : std::string();
        auto json = StoryJson(s, node, offset, limit, true, next);
        if (GameToolReplyBytes(json) <= kGameToolReplyBudget)
            return json;
        if (limit > 1)
            --limit;
        else
            return "{\"error\":\"reply_budget_exceeded\"}";
    }
}
}  // namespace spring
