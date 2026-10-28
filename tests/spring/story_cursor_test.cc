#include "game/spring_tool_json.h"
#include <cassert>
#include <iostream>
using namespace spring;
void emit(const char* name, const std::string& text) {
    assert(GameToolReplyBytes(text) <= kGameToolReplyBudget);
    std::cout << "{\"case\":" << StoryQuote(name) << ",\"text\":" << StoryQuote(text.c_str()) << "}\n";
}
int main() {
    Core c;
    c.storage_ok = true;
    c.EnableEconomy(true);
    c.EnableMemory(1, 42);
    c.EnableStory();
    c.s.story.count = 0;
    for (unsigned i = 0; i < 16; ++i)
        c.StoryRecord(i, 6, 1000 + i);
    const auto initial = c.Get();
    const auto before = c.PackStory()->checksum;
    emit("initial-first", ToolStoryJson(initial, 255, 0, 6));
    emit("initial-second", ToolStoryJson(initial, 255, 0, 6, StoryRecordCursor(initial, 6)));
    emit("initial-last", ToolStoryJson(initial, 255, 0, 6, StoryRecordCursor(initial, 12)));
    emit("legacy-offset-rejected", ToolStoryJson(initial, 255, 6, 6));
    emit("mismatched-offset-rejected", ToolStoryJson(initial, 255, 5, 6, StoryRecordCursor(initial, 6)));
    for (const auto& cursor : {std::string("bad"), std::string(1000, 'x'), StoryRecordCursor(initial, 16)})
        assert(ToolStoryJson(initial, 255, 0, 6, cursor).find("restart_required\":true") != std::string::npos);
    assert(c.PackStory()->checksum == before);
    // A live append shifts a full ring buffer. An old cursor must never return
    // position6 from the changed buffer (the original skipped-record failure).
    c.StoryRecord(23, 6, 2000);
    emit("after-append-stale", ToolStoryJson(c.Get(), 255, 0, 6, StoryRecordCursor(initial, 6)));
    emit("after-append-first", ToolStoryJson(c.Get(), 255, 0, 6));
    c.StoryRecord(22, 6, 2001);
    emit("after-two-appends-stale", ToolStoryJson(c.Get(), 255, 0, 6, StoryRecordCursor(initial, 6)));
    auto other = initial;
    other.demo = true;
    emit("other-mode", ToolStoryJson(other, 255, 0, 6, StoryRecordCursor(initial, 6)));
    other = initial;
    other.memory = std::make_shared<MemoryData>(*initial.memory);
    ++other.memory->world_lo;
    emit("other-world", ToolStoryJson(other, 255, 0, 6, StoryRecordCursor(initial, 6)));
    auto revision_changed = initial;
    ++revision_changed.story.revision;
    emit("same-count-new-version", ToolStoryJson(revision_changed, 255, 0, 6, StoryRecordCursor(initial, 6)));
    auto wrapped = initial;
    wrapped.story.revision = 0xffffffffu;
    const auto cursor_before_wrap = StoryRecordCursor(wrapped, 6);
    wrapped.story.revision = 0;
    emit("revision-wrap", ToolStoryJson(wrapped, 255, 0, 6, cursor_before_wrap));
    auto empty = initial;
    empty.story.count = 0;
    emit("empty", ToolStoryJson(empty, 255, 0, 6));
    auto short_page = initial;
    short_page.story.count = 3;
    emit("short", ToolStoryJson(short_page, 255, 0, 6));
    auto restored = Core{};
    restored.storage_ok = true;
    assert(restored.RestoreStory(*c.PackStory()));
    assert(restored.s.story.revision == c.s.story.revision);
    const auto current = c.Get();
    emit("save-roundtrip-cursor", ToolStoryJson(restored.Get(), 255, 0, 6, StoryRecordCursor(current, 6)));
}
