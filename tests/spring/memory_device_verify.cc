#include "game/spring_memory_json.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){
 assert(argc==3);
 auto archive=std::make_unique<spring::ArchiveSave>();
 spring::CheckpointSave checkpoint{};
 std::ifstream a(argv[1],std::ios::binary),b(argv[2],std::ios::binary);
 a.read(reinterpret_cast<char*>(archive->data()),sizeof(*archive));
 b.read(reinterpret_cast<char*>(checkpoint.data()),sizeof(checkpoint));
 assert(a.gcount()==sizeof(*archive)&&b.gcount()==sizeof(checkpoint));
 spring::Core c;c.storage_ok=true;
 assert(c.RestoreArchive(*archive));
 assert(c.RestoreCheckpoint(checkpoint));
 assert(c.s.memory&&c.s.memory->count&&c.s.memory->fact_count);

 std::cout<<spring::MemoryJson(c.Get())<<'\n';
}
