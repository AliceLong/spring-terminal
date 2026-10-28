#include "game/spring_core.h"
#include <fstream>
#include <cassert>
#include <iostream>
int main(int argc,char**argv){assert(argc==2);spring::FullSave words{};std::ifstream file(argv[1],std::ios::binary);file.read(reinterpret_cast<char*>(words.data()),sizeof(words));assert(file.gcount()==sizeof(words));spring::Core c;c.storage_ok=true;assert(c.RestoreFull(words));assert(c.s.economy.enabled);assert(c.PackFull()==words);std::cout<<"Device1536-byte economy save: verified, restored and byte-identical on re-pack.\n";}
