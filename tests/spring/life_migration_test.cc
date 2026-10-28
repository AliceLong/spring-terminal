#include "game/spring_core.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
  assert(argc == 2);
  spring::FullSave old{};
  std::ifstream file(argv[1], std::ios::binary);
  file.read(reinterpret_cast<char *>(old.data()), sizeof(old));
  assert(file.gcount() == sizeof(old) && old[129] == 3);
  const auto immutable = old;
  spring::Core c;
  c.storage_ok = true;
  assert(c.RestoreFull(old));
  assert(!c.s.life.sequence && c.s.life.cooldown == 120000);
  const auto migrated = c.PackFull();
  assert(migrated[129] == 4 && old == immutable);
  // All old serialized world/economy words retained except format and checksum.
  for (unsigned i = 0; i < 252; ++i)
    if (i != 129)
      assert(old[i] == migrated[i]);
  spring::Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreFull(migrated));
  assert(restored.PackFull() == migrated);
  std::cout << "Actual schema3 device save migrated to schema4; previous state "
               "words unchanged; "
               "new save round-trip passed; source untouched.\n";
}
