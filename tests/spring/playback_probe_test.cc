#include "audio/playback_probe.h"
#include <cassert>
#include <iostream>

int main() {
    audio::PlaybackProbe p;
    p.Packet(100); // Background sounds and inactive sessions are excluded.
    assert(p.packets == 0);
    p.Begin();
    const auto first = p.turn;
    p.Wait(first, 1000000, false); // Initial TTS latency is not starvation.
    assert(p.wait_count == 0);
    p.Packet(1000000);
    p.Packet(1060000);
    assert(p.packet_gap_max_us == 60000);
    p.Output(first, 60000, 60000, 32768, 2);
    p.Wait(first, 4999, false);
    p.Wait(first, 20000, true);
    p.Wait(first, 30000, false);
    assert(p.wait_count == 2 && p.wait_encoded_pending == 1);
    assert(p.wait_max_us == 30000 && p.wait_total_us == 50000);
    p.Decoded(first, 9000, false);
    p.Output(first, 80000, 60000, 24000, 0);
    assert(p.decode_errors == 1 && p.decode_max_us == 9000);
    assert(p.late_writes == 1 && p.peak == 32768 && p.clipped_samples == 2);
    p.End();
    p.Wait(first, 4000000, false); // Normal end or aborted TTS is excluded.
    assert(p.wait_count == 2);
    p.Begin();
    p.Output(first, 9999999, 60000, 32768, 100); // A stale task cannot pollute a new turn.
    p.Wait(first, 9999999, false);
    p.Packet(9000000); // Silence between turns must not inflate the packet gap.
    assert(p.frames == 2 && p.wait_count == 2 && p.packet_gap_max_us == 60000);
    audio::PlaybackDiagnostics s{p, 3, 1, true, false};
    std::cout << audio::PlaybackDiagnosticsJson(s) << '\n';
}
