#include "pstouch/photo_filters.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <set>
int main() {
    using namespace pstouch;
    std::set<std::string> names;
    for(uint32_t i=0;i<photo_preset_count;++i) {
        const auto preset=static_cast<PhotoPreset>(i);
        names.insert(photo_preset_name(preset));
        Image sample(2,1,{90,130,170,255});
        sample.at(1,0)={220,80,40,96};
        const auto alpha0=sample.at(0,0).a, alpha1=sample.at(1,0).a;
        apply_photo_preset(sample,preset);
        assert(sample.at(0,0).a==alpha0&&sample.at(1,0).a==alpha1);
    }
    assert(names.size()==100);
    assert(photo_preset_name(PhotoPreset::Warm01)=="Warm 01");
    Image base(1,1,{80,120,160,200}), altered=base;
    apply_photo_preset(altered,PhotoPreset::Monochrome10);
    assert(altered.at(0,0).r==altered.at(0,0).g&&altered.at(0,0).g==altered.at(0,0).b);
    assert(altered.at(0,0).a==200);
    bool threw=false;
    try { apply_photo_preset(base,static_cast<PhotoPreset>(255)); } catch(const std::invalid_argument&) { threw=true; }
    assert(threw);
    std::cout<<"PASS: 100 photo presets, unique names, monochrome grading, alpha preservation and invalid preset guard\n";
}
