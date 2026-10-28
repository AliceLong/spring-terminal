#include "spring_scenery.h"
#include <cassert>
#include <iostream>
int main() {
    unsigned count = 0;
    unsigned visible[4]{}, moved[4]{};
    for (int y = 0; y < 480; ++y) for (int x = 0; x < 480; ++x) {
        if (!spring_scenery::Glass(x,y)) continue;
        assert(y >= spring_scenery::kTop && y < spring_scenery::kTop+spring_scenery::kHeight);
        ++count;
        for (unsigned r = 1; r < 5; ++r) {
            auto far = spring_scenery::Far(r,x,y);
            assert(far.a == 255 && far.r <= 255 && far.g <= 255 && far.b <= 255);
            const auto n0 = spring_scenery::Near(r,x,y,0);
            const auto n1 = spring_scenery::Near(r,x,y,160);
            if (n0.a >= 128) ++visible[r-1];
            if ((n0.a || n1.a) && (n0.r!=n1.r || n0.g!=n1.g || n0.b!=n1.b || n0.a!=n1.a)) ++moved[r-1];
            for (unsigned offset : {0u,719u,720u,1439u,1440u}) {
                auto near = spring_scenery::Near(r,x,y,offset);
                assert(near.r <= 255 && near.g <= 255 && near.b <= 255 && near.a <= 255);
            }
        }
    }
    for (unsigned r=0;r<4;++r) {
        std::cout << "region " << r+1 << " visible=" << visible[r] << " moving=" << moved[r] << "\n";
        assert(visible[r] > 5000 && moved[r] > 5000);
    }
    assert(count > 30000 && count < 52000);
    unsigned occluded=0;
    for(int y=129;y<235;++y) for(int x=0;x<140;++x)
        if(spring_scenery::Rack(x,y)) { assert(!spring_scenery::Glass(x,y)); ++occluded; }
    assert(occluded>2000);
    assert(!spring_scenery::Glass(150,100) && !spring_scenery::Glass(340,200));
    assert(!spring_scenery::Glass(200,240) && !spring_scenery::Glass(44,88));
    std::cout << "Four region decoders and reflected boundary samples safe; window frame excluded.\n";
}
