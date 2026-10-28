#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace audio {
// Caller holds the audio queue mutex. These are software observations, not DMA underrun IRQs.
struct PlaybackProbe {
    bool active = false, played = false;
    uint32_t turn = 0, packets = 0, rejected = 0, decode_errors = 0, frames = 0;
    uint32_t wait_count = 0, wait_encoded_pending = 0, late_writes = 0;
    uint64_t wait_total_us = 0, wait_max_us = 0, packet_gap_max_us = 0;
    uint64_t decode_max_us = 0, write_max_us = 0, clipped_samples = 0;
    uint32_t peak = 0;
    int64_t last_packet_us = 0;

    void Begin() {
        ++turn;
        active = true;
        played = false;
        last_packet_us = 0;
    }
    void End() { active = false; }
    void Packet(int64_t now) {
        if (!active)
            return;
        ++packets;
        if (last_packet_us)
            packet_gap_max_us = std::max(packet_gap_max_us, uint64_t(now - last_packet_us));
        last_packet_us = now;
    }
    void Wait(uint32_t epoch, uint64_t elapsed, bool encoded_pending) {
        if (!active || !played || epoch != turn || elapsed < 5000)
            return;
        ++wait_count;
        wait_encoded_pending += encoded_pending;
        wait_total_us += elapsed;
        wait_max_us = std::max(wait_max_us, elapsed);
    }
    void Decoded(uint32_t epoch, uint64_t elapsed, bool ok) {
        if (!active || epoch != turn)
            return;
        decode_max_us = std::max(decode_max_us, elapsed);
        decode_errors += !ok;
    }
    void Output(uint32_t epoch, uint64_t elapsed, uint64_t duration, uint32_t pcm_peak,
                uint32_t clipped) {
        if (!active || epoch != turn)
            return;
        played = true;
        ++frames;
        write_max_us = std::max(write_max_us, elapsed);
        late_writes += elapsed > duration + 10000;
        peak = std::max(peak, pcm_peak);
        clipped_samples += clipped;
    }
};

struct PlaybackDiagnostics {
    PlaybackProbe totals;
    size_t encoded_queue = 0, pcm_queue = 0;
    bool decoding = false, writing = false;
};
inline std::string PlaybackDiagnosticsJson(const PlaybackDiagnostics& s) {
    const auto& p = s.totals;
    std::ostringstream o;
    o << "{\"scope\":\"boot_cumulative_tts_active_only\",\"software_observations\":true"
      << ",\"tts_active\":" << (p.active ? "true" : "false") << ",\"turn\":" << p.turn
      << ",\"packets\":" << p.packets << ",\"queue_rejected\":" << p.rejected
      << ",\"decode_errors\":" << p.decode_errors << ",\"played_frames\":" << p.frames
      << ",\"pcm_wait_count\":" << p.wait_count << ",\"pcm_wait_total_us\":" << p.wait_total_us
      << ",\"pcm_wait_max_us\":" << p.wait_max_us
      << ",\"wait_encoded_pending\":" << p.wait_encoded_pending
      << ",\"packet_gap_max_us\":" << p.packet_gap_max_us
      << ",\"decode_max_us\":" << p.decode_max_us << ",\"write_max_us\":" << p.write_max_us
      << ",\"late_writes\":" << p.late_writes << ",\"pcm_peak\":" << p.peak
      << ",\"full_scale_samples\":" << p.clipped_samples << ",\"encoded_queue\":" << s.encoded_queue
      << ",\"pcm_queue\":" << s.pcm_queue << ",\"decoding\":" << (s.decoding ? "true" : "false")
      << ",\"writing\":" << (s.writing ? "true" : "false") << '}';
    return o.str();
}
}  // namespace audio
