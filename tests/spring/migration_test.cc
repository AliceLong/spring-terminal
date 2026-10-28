#include "game/spring_core.h"
#include <fstream>
#include <cassert>
#include <iostream>
int main(int argc,char** argv){assert(argc==2);spring::Save save{};std::ifstream file(argv[1],std::ios::binary);file.read(reinterpret_cast<char*>(save.data()),sizeof(save));assert(file.gcount()==sizeof(save));spring::Core c;c.storage_ok=true;assert(c.Restore(save));const auto water=c.s.water,energy=c.s.energy,fiber=c.s.fiber,tree=c.s.tree_stage;c.EnableEconomy(false);assert(c.s.water==water&&c.s.energy==energy&&c.s.fiber==fiber&&c.s.tree_stage==tree);spring::Core r;r.storage_ok=true;assert(r.RestoreFull(c.PackFull()));assert(r.s.water==water&&r.s.energy==energy&&r.s.fiber==fiber&&r.s.tree_stage==tree);std::cout<<"Actual device legacy save migrated/restored without changing existing resources or tree.\n";}
