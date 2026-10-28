#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace spring {
// USB accepts only bounded hexadecimal words, never model prose or game commands.
struct MindPacket {
    uint32_t boot = 0, offer = 0, world_hi = 0, world_lo = 0, sequence = 0, intent = 0, target = 0,
             social_record = 0;
    uint32_t social_kind = 0, focus_mode = 0, focus_kind = 0, focus_source = 0, focus_target = 0,
             extended = 0;
};
struct MindReplyGuard {
    uint32_t boot, offer, world_hi, world_lo, sequence;
    uint64_t deadline_ms;
    bool Fresh(const MindPacket& p, uint64_t now) const {
        return deadline_ms && now < deadline_ms && p.boot == boot && p.offer == offer &&
               p.world_hi == world_hi && p.world_lo == world_lo;
    }
    bool BodyAvailable(const MindPacket& p, uint64_t now, uint32_t mask) const {
        return Fresh(p, now) && p.sequence == sequence && mask && p.intent <= 3 && p.target <= 3 &&
               (!p.intent || (mask & (1u << ((p.intent - 1) * 4 + p.target))));
    }
};
class MindLine {
    std::array<char, 160> line{};
    unsigned length = 0;
    bool overflow = false;

public:
    bool Feed(char c, MindPacket& out) {
        if (c == '\r')
            return false;
        if (c != '\n') {
            if (length + 1 < line.size())
                line[length++] = c;
            else
                overflow = true;
            return false;
        }
        line[length] = 0;
        const bool valid = !overflow && Parse(line.data(), out);
        length = 0;
        overflow = false;
        return valid;
    }
    static bool Parse(const char* p, MindPacket& out) {
        const bool extended = !std::strncmp(p, "MIND3 ", 6);
        const bool social = !std::strncmp(p, "MIND2 ", 6);
        if (!extended && !social && std::strncmp(p, "MIND1 ", 6))
            return false;
        p += 6;
        uint32_t words[13]{};
        const unsigned count = extended ? 13 : social ? 8 : 7;
        for (unsigned i = 0; i < count; ++i) {
            unsigned n = 0;
            while (*p && *p != ' ') {
                const unsigned d = *p >= '0' && *p <= '9'   ? *p - '0'
                                   : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10
                                                            : 16;
                if (d > 15 || ++n > 8)
                    return false;
                words[i] = (words[i] << 4) | d;
                ++p;
            }
            if (!n || (i + 1 < count ? *p != ' ' : *p != 0))
                return false;
            if (i + 1 < count)
                ++p;
        }
        if (words[5] > 3 || (words[5] != 2 && words[6]) || words[6] > 3)
            return false;
        if (extended && (words[8] > 3 || words[9] > 2 || words[10] > 4 || words[12] > 31))
            return false;
        out = {words[0],  words[1],          words[2],
               words[3],  words[4],          words[5],
               words[6],  words[7],          extended ? words[8] : (words[7] ? 1u : 0u),
               words[9],  words[10],         words[11],
               words[12], extended ? 1u : 0u};
        return true;
    }
};
}  // namespace spring
