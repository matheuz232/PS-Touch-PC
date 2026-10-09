#include "pstouch/photo_filters.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace pstouch {
namespace {
constexpr std::array<const char*,10> kFamilies{"Natural", "Warm", "Cool", "Vintage", "Cinema", "Fade", "Vivid", "Matte", "TealOrange", "Monochrome"};
uint8_t byte(float value) {
    if (!std::isfinite(value)) return 0;
    return static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}
}
std::string photo_preset_name(PhotoPreset preset) {
    const auto index=static_cast<uint32_t>(preset);
    if(index>=photo_preset_count) throw std::invalid_argument("unknown photo preset");
    const auto family=index/10U, strength=index%10U+1U;
    std::string name=kFamilies[family];
    name.push_back(' ');
    if(strength<10U) name.push_back('0');
    name+=std::to_string(strength);
    return name;
}
void apply_photo_preset(Image& image, PhotoPreset preset) {
    const auto index=static_cast<uint32_t>(preset);
    if(index>=photo_preset_count) throw std::invalid_argument("unknown photo preset");
    const auto family=index/10U;
    const float strength=0.18f+static_cast<float>(index%10U)*0.055f;
    float saturation=1.0f, contrast=1.0f, warmth=0.0f, tint=0.0f, fade=0.0f;
    switch(family) {
    case 0: saturation=1.0f+strength*0.12f; contrast=1.0f+strength*0.08f; break;
    case 1: warmth=strength*24.0f; saturation=1.0f+strength*0.08f; break;
    case 2: warmth=-strength*22.0f; saturation=1.0f+strength*0.05f; break;
    case 3: warmth=strength*18.0f; tint=-strength*4.0f; saturation=1.0f-strength*0.28f; contrast=1.0f-strength*0.12f; fade=strength*0.10f; break;
    case 4: warmth=strength*8.0f; tint=-strength*7.0f; saturation=1.0f+strength*0.18f; contrast=1.0f+strength*0.24f; break;
    case 5: saturation=1.0f-strength*0.25f; contrast=1.0f-strength*0.18f; fade=strength*0.25f; break;
    case 6: saturation=1.0f+strength*0.75f; contrast=1.0f+strength*0.10f; break;
    case 7: saturation=1.0f-strength*0.12f; contrast=1.0f-strength*0.24f; fade=strength*0.18f; tint=strength*3.0f; break;
    case 8: warmth=strength*12.0f; tint=-strength*12.0f; saturation=1.0f+strength*0.22f; contrast=1.0f+strength*0.16f; break;
    case 9: saturation=0.0f; contrast=1.0f+strength*0.10f; break;
    default: throw std::invalid_argument("unknown photo preset");
    }
    for(auto& p:image.mutable_pixels()) {
        const float r=p.r, g=p.g, b=p.b;
        const float luminance=0.2126f*r+0.7152f*g+0.0722f*b;
        float nr=luminance+(r-luminance)*saturation;
        float ng=luminance+(g-luminance)*saturation;
        float nb=luminance+(b-luminance)*saturation;
        nr=(nr-127.5f)*contrast+127.5f+warmth+tint;
        ng=(ng-127.5f)*contrast+127.5f+tint;
        nb=(nb-127.5f)*contrast+127.5f-warmth+tint;
        // Lift shadows without clipping highlights; alpha is intentionally untouched.
        nr=nr*(1.0f-fade)+255.0f*fade*0.45f;
        ng=ng*(1.0f-fade)+255.0f*fade*0.45f;
        nb=nb*(1.0f-fade)+255.0f*fade*0.45f;
        p.r=byte(nr); p.g=byte(ng); p.b=byte(nb);
    }
}
}
