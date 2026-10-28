"""Exercise the actual device loader with failed allocations and corrupt flash fixtures."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT/'vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16'

CHECK = '''#pragma once
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_ERR_NOT_FOUND=1, ESP_ERR_INVALID_SIZE=2;
constexpr int ESP_ERR_INVALID_RESPONSE=3, ESP_ERR_INVALID_CRC=4, ESP_ERR_NO_MEM=5;
#define ESP_RETURN_ON_FALSE(c,e,...) do { if (!(c)) return e; } while(0)
#define ESP_RETURN_ON_ERROR(c,...) do { int e=(c); if(e) return e; } while(0)
'''
PARTITION = '''#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_check.h"
struct esp_partition_t { size_t size; };
using esp_partition_mmap_handle_t = uint32_t;
constexpr int ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_ANY=0;
constexpr int ESP_PARTITION_MMAP_DATA=0;
const esp_partition_t* esp_partition_find_first(int,int,const char*);
int esp_partition_read(const esp_partition_t*,size_t,void*,size_t);
int esp_partition_mmap(const esp_partition_t*,size_t,size_t,int,const void**,uint32_t*);
void esp_partition_munmap(uint32_t);
'''
HEAP = '''#pragma once
#include <cstddef>
constexpr int MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2;
size_t heap_caps_get_free_size(int);
size_t heap_caps_get_largest_free_block(int);
void* heap_caps_malloc(size_t,int);
void heap_caps_free(void*);
'''
MAIN = r'''
#include "spring_external_art.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include <zlib.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
constexpr size_t N=480*480*2;
std::vector<uint8_t> flash;
std::string scenario;
int maps=0, unmaps=0, allocations=0, frees=0, crcs=0;
esp_partition_t partition{8*1024*1024};
const esp_partition_t* esp_partition_find_first(int,int,const char*) { return &partition; }
int esp_partition_read(const esp_partition_t*,size_t off,void* out,size_t n) {
    if(off+n>flash.size()) return ESP_ERR_INVALID_SIZE;
    memcpy(out,flash.data()+off,n); return ESP_OK;
}
int esp_partition_mmap(const esp_partition_t*,size_t off,size_t n,int,const void** out,uint32_t* h) {
    ++maps;
    if(off+n>flash.size()) return ESP_ERR_INVALID_SIZE;
    *out=flash.data()+off; *h=1; return ESP_OK;
}
void esp_partition_munmap(uint32_t) { ++unmaps; }
size_t heap_caps_get_free_size(int c) {
    assert(c==MALLOC_CAP_SPIRAM);
    return scenario=="budget" ? N+2*1024*1024-1 : 4*1024*1024;
}
size_t heap_caps_get_largest_free_block(int c) {
    assert(c==MALLOC_CAP_SPIRAM);
    return scenario=="fragmented" ? N-4 : 3*1024*1024;
}
void* heap_caps_malloc(size_t n,int c) {
    assert(n==N && c==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    ++allocations; return scenario=="oom" ? nullptr : malloc(n);
}
void heap_caps_free(void* p) { ++frees; free(p); }
uint32_t esp_rom_crc32_le(uint32_t c,const uint8_t* p,uint32_t n) {
    ++crcs;
    auto crc=uint32_t(crc32(c,p,n));
    return scenario=="copy_crc" && crcs==2 ? crc^1 : crc;
}
int main(int argc,char** argv) {
    assert(argc==2); scenario=argv[1];
    struct Header { uint32_t count,checksum,length; } header{1,0,uint32_t(48+N)};
    struct Entry { char name[32]; uint32_t size,offset; uint16_t w,h; } entry{};
    static_assert(sizeof(entry)==44);
    strcpy(entry.name,"spring_background.rgb565");entry.size=N;entry.offset=2;entry.w=entry.h=480;
    if(scenario=="missing") strcpy(entry.name,"different.bin");
    if(scenario=="overflow") entry.offset=0xfffffff0;
    if(scenario=="unaligned") entry.offset=1;
    if(scenario=="count") header.count=257;
    flash.resize(60+N,0);memcpy(flash.data(),&header,12);memcpy(flash.data()+12,&entry,44);
    flash[58]=flash[59]='Z';
    for(size_t i=0;i<N;++i) flash[60+i]=i%251;
    auto crc=uint32_t(crc32(0,flash.data()+60,N));
    if(scenario=="crc") crc^=1;
    if(scenario=="truncated") flash.resize(100);
    const auto* p=spring_art::LoadExternalBackground("spring_background.rgb565",N,crc);
    const auto info=spring_art::GetBackgroundMemory();
    assert((p!=nullptr)==(scenario=="ok"));
    if(p) {
        assert(memcmp(p,flash.data()+60,N)==0);
        assert(p!=flash.data()+60 && maps==1 && unmaps==1 && allocations==1);
        assert(info.ready && info.bytes==N && info.allocations==1);
        flash[60]^=1; assert(p[0]==0); // Independent, retained PSRAM pixels.
    } else {
        assert(!info.ready && info.bytes==0);
        assert(info.allocations==(scenario=="copy_crc" ? 1u : 0u));
    }
    const int before_alloc=allocations, before_maps=maps;
    assert(spring_art::LoadExternalBackground("spring_background.rgb565",N,crc)==p);
    assert(allocations==before_alloc && maps==before_maps); // No per-frame retries/allocations.
    if(scenario=="budget" || scenario=="fragmented") assert(allocations==0 && unmaps==1);
    if(scenario=="copy_crc") assert(frees==1 && unmaps==1);
    if(p) free(const_cast<uint8_t*>(p));
}
'''


class BackgroundLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.path = Path(cls.tmp.name)
        headers = {'esp_check.h': CHECK, 'esp_partition.h': PARTITION, 'esp_heap_caps.h': HEAP,
                   'esp_log.h': '#pragma once\n#define ESP_LOGE(t,...) ((void)(t))\n#define ESP_LOGI(t,...) ((void)(t))\n',
                   'esp_rom_crc.h': '#pragma once\n#include <cstdint>\nuint32_t esp_rom_crc32_le(uint32_t,const uint8_t*,uint32_t);\n'}
        for name, body in headers.items():
            (cls.path/name).write_text(body)
        (cls.path/'test.cc').write_text(MAIN)
        cls.binary = cls.path/'test'
        subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-DSPRING_BACKGROUND_PSRAM',
                        '-I'+str(cls.path), '-I'+str(BOARD), str(BOARD/'spring_external_art.cc'),
                        str(cls.path/'test.cc'), '-lz', '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_faults_and_once_only_psram_copy(self):
        for case in ['ok', 'budget', 'fragmented', 'oom', 'crc', 'copy_crc', 'missing',
                     'overflow', 'unaligned', 'count', 'truncated']:
            with self.subTest(case=case):
                subprocess.run([str(self.binary), case], check=True)


if __name__ == '__main__':
    unittest.main()
