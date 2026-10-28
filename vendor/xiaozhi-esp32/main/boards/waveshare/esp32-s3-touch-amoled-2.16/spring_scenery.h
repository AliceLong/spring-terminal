#pragma once
#include <cstdint>
namespace spring_scenery {
struct Region { const uint8_t* far; const uint8_t* near; };
struct Color { unsigned r, g, b, a; };
extern const Region kRegions[4];
extern const uint8_t kRackMask[];
constexpr int kTop = 88, kHeight = 160;
constexpr int kNearTop = 128, kNearHeight = 128, kNearWidth = 480;
inline bool Rack(int x, int y) {
    if (x < 0 || x >= 140 || y < 129 || y >= 235) return false;
    return (kRackMask[(y-129)*18+x/8] & (128>>(x%8))) != 0;
}
inline bool Glass(int x, int y) {
    if (Rack(x,y)) return false;
    // Rounded inner glass; do not overwrite the metal window frame.
    const int boxes[3][4] = {{44,88,89,147},{153,88,174,147},{347,88,87,147}};
    for (const auto& b : boxes) {
        if (x < b[0] || x >= b[0]+b[2] || y < b[1] || y >= b[1]+b[3]) continue;
        const int cx = x < b[0]+7 ? b[0]+7 : x >= b[0]+b[2]-7 ? b[0]+b[2]-8 : x;
        const int cy = y < b[1]+7 ? b[1]+7 : y >= b[1]+b[3]-7 ? b[1]+b[3]-8 : y;
        return (x-cx)*(x-cx)+(y-cy)*(y-cy) <= 49;
    }
    return false;
}
inline Color Palette(const uint8_t* p, unsigned index) {
    p += index * 4; return {p[0],p[1],p[2],p[3]};
}
inline Color Far(unsigned region, int x, int y) {
    const auto* p = kRegions[region-1].far;
    const int u = x*127*256/479, v = (y-40)*63*256/239;
    const int sx = u>>8, sy = v>>8, nx = sx < 127 ? sx+1 : sx, ny = sy < 63 ? sy+1 : sy;
    const auto a = Palette(p,p[1024+sy*128+sx]);
    const auto b = Palette(p,p[1024+sy*128+nx]);
    const auto c = Palette(p,p[1024+ny*128+sx]);
    const auto d = Palette(p,p[1024+ny*128+nx]);
    const unsigned fx = u&255, fy = v&255;
    const auto mix = [fx,fy](unsigned a, unsigned b, unsigned c, unsigned d) {
        return ((a*(256-fx)+b*fx)*(256-fy)+(c*(256-fx)+d*fx)*fy+32768)>>16;
    };
    return {mix(a.r,b.r,c.r,d.r),mix(a.g,b.g,c.g,d.g),mix(a.b,b.b,c.b,d.b),255};
}
inline Color NearPixel(unsigned region, int sx, int sy) {
    const auto* packed = kRegions[region-1].near+256+sy*120+(sx/4)*3;
    unsigned index;
    switch (sx%4) {
    case 0: index=packed[0]>>2; break;
    case 1: index=((packed[0]&3)<<4)|(packed[1]>>4); break;
    case 2: index=((packed[1]&15)<<2)|(packed[2]>>6); break;
    default: index=packed[2]&63; break;
    }
    return Palette(kRegions[region-1].near,index);
}
inline Color Near(unsigned region, int x, int y, unsigned offset) {
    if (y < kNearTop || y >= kNearTop+kNearHeight) return {0,0,0,0};
    const int u = (static_cast<unsigned>(x)+offset)%(kNearWidth*2);
    const int reflected = u < kNearWidth ? u : kNearWidth*2-1-u;
    // Keep foreground edges crisp; only the distant panorama is interpolated.
    return NearPixel(region,reflected*160/kNearWidth,(y-kNearTop)*48/kNearHeight);
}
}
