#include "spring_roadshow.h"
#include "spring_scene_input.h"
#include <cassert>
#include <cstdio>

int main() {
  for (unsigned scene : {1u, 8u, 9u, 10u, 11u}) {
    spring::Core game;
    assert(spring_show::Prepare(game, scene, 100, scene));
    game.roadshow_epoch = 1;
    std::atomic<uint32_t> input{0};
    uint32_t displayed = 0;
    bool stale_touch_queued = true;
    unsigned clears = 0;
    const uint32_t old_command_epoch = input.load();
    assert(old_command_epoch != game.roadshow_epoch);
    auto render = [&] {
      return spring_ui::SyncSceneInput(
          game.roadshow_epoch, displayed, input, [&] {
            // Admission stays closed until stale input has actually been
            // discarded.
            assert(input.load() != game.roadshow_epoch);
            stale_touch_queued = false;
            ++clears;
          });
    };
    assert(render());
    assert(!stale_touch_queued && clears == 1);
    assert(input.load() == game.roadshow_epoch);
    assert(old_command_epoch !=
           game.roadshow_epoch); // Old commands remain rejected.
    // No plant-dialog click is needed. Reading an unlocked archive or accepting
    // a story is legal while routine watering is still running.
        game.work = spring::Work::Water;
    assert(game.ReadStoryArchive(0));
    const unsigned node = scene == 1 ? 0 : scene == 8 ? 2 : scene == 11 ? 6 : 3;
    assert(game.AcceptStory(node));
    assert(!render() && clears == 1); // Don't repeatedly close the opened card.
    game.roadshow_epoch =
        2; // A later switch/return needs the same reset ordering.
    stale_touch_queued = true;
    assert(render() && !stale_touch_queued && clears == 2);
  }
  std::puts("Scene input: first-render communication admission, stale click "
            "rejection, "
            "watering concurrency and subsequent context switches passed.");
}
