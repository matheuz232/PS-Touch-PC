#include "pstouch/image.hpp"
#include <algorithm>
#include <utility>
#include <cmath>
#include <limits>

namespace pstouch {
namespace {
uint8_t clamp_byte(float v) {
    if (!std::isfinite(v)) return 0;
    return static_cast<uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}
}
Image::Image(uint32_t width, uint32_t height, Pixel fill) : width_(width), height_(height) {
    if (!width || !height) throw std::invalid_argument("image dimensions must be non-zero");
    const uint64_t count = static_cast<uint64_t>(width) * height;
    if (count > 100000000ULL || count > std::numeric_limits<size_t>::max() / sizeof(Pixel))
        throw std::length_error("image dimensions exceed safety limit");
    pixels_.assign(static_cast<size_t>(count), fill);
}
Pixel& Image::at(uint32_t x, uint32_t y) {
    if (x >= width_ || y >= height_) throw std::out_of_range("pixel coordinate out of range");
    return pixels_[static_cast<size_t>(y) * width_ + x];
}
const Pixel& Image::at(uint32_t x, uint32_t y) const {
    if (x >= width_ || y >= height_) throw std::out_of_range("pixel coordinate out of range");
    return pixels_[static_cast<size_t>(y) * width_ + x];
}
void premultiply_alpha(Image& image) {
    for (auto& p : image.mutable_pixels()) {
        const float a = p.a / 255.0f;
        p.r = clamp_byte(p.r * a); p.g = clamp_byte(p.g * a); p.b = clamp_byte(p.b * a);
    }
}
void unpremultiply_alpha(Image& image) {
    for (auto& p : image.mutable_pixels()) {
        if (p.a == 0) { p.r = p.g = p.b = 0; continue; }
        const float k = 255.0f / p.a;
        p.r = clamp_byte(p.r * k); p.g = clamp_byte(p.g * k); p.b = clamp_byte(p.b * k);
    }
}
void source_over(Image& dst, const Image& src, int32_t ox, int32_t oy) {
    for (uint32_t sy = 0; sy < src.height(); ++sy) for (uint32_t sx = 0; sx < src.width(); ++sx) {
        const int64_t dx = static_cast<int64_t>(ox) + sx, dy = static_cast<int64_t>(oy) + sy;
        if (dx < 0 || dy < 0 || dx >= dst.width() || dy >= dst.height()) continue;
        const Pixel s = src.at(sx, sy); Pixel& d = dst.at(static_cast<uint32_t>(dx), static_cast<uint32_t>(dy));
        const float sa = s.a / 255.0f, da = d.a / 255.0f, oa = sa + da * (1.0f - sa);
        if (oa <= 0.0f) { d = {0,0,0,0}; continue; }
        const auto blend = [&](uint8_t sc, uint8_t dc) { return clamp_byte((sc * sa + dc * da * (1.0f - sa)) / oa); };
        d.r = blend(s.r, d.r); d.g = blend(s.g, d.g); d.b = blend(s.b, d.b); d.a = clamp_byte(oa * 255.0f);
    }
}
Image resample_bilinear(const Image& src, uint32_t width, uint32_t height) {
    Image out(width, height);
    for (uint32_t y = 0; y < height; ++y) {
        const float fy = std::clamp((static_cast<float>(y) + 0.5f) * static_cast<float>(src.height()) / static_cast<float>(height) - 0.5f, 0.0f, static_cast<float>(src.height()-1));
        const uint32_t y0 = static_cast<uint32_t>(fy), y1 = std::min(y0 + 1, src.height()-1); const float ty = fy-static_cast<float>(y0);
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = std::clamp((static_cast<float>(x) + 0.5f) * static_cast<float>(src.width()) / static_cast<float>(width) - 0.5f, 0.0f, static_cast<float>(src.width()-1));
            const uint32_t x0 = static_cast<uint32_t>(fx), x1 = std::min(x0 + 1, src.width()-1); const float tx = fx-static_cast<float>(x0);
            const Pixel p00=src.at(x0,y0), p10=src.at(x1,y0), p01=src.at(x0,y1), p11=src.at(x1,y1);
            auto channel = [&](uint8_t Pixel::*m) { const float a=p00.*m+(p10.*m-p00.*m)*tx, b=p01.*m+(p11.*m-p01.*m)*tx; return clamp_byte(a+(b-a)*ty); };
            out.at(x,y) = {channel(&Pixel::r),channel(&Pixel::g),channel(&Pixel::b),channel(&Pixel::a)};
        }
    }
    return out;
}
void brightness_contrast(Image& image, float brightness, float contrast) {
    if (!std::isfinite(brightness) || !std::isfinite(contrast)) throw std::invalid_argument("brightness/contrast must be finite");
    brightness = std::clamp(brightness, -1.0f, 1.0f);
    contrast = std::clamp(contrast, -1.0f, 1.0f);
    // Mirrors the formula in assets/resource/application/filters/contrastbrightness.fs.
    // Input/output channels are normalized RGBA values, with RGB constrained by alpha.
    const float wContrast = contrast * contrast * (contrast < 0.0f ? -1.0f : 1.0f);
    const float wBrightness = brightness * brightness * (brightness < 0.0f ? -1.0f : 1.0f);
    for (auto& p : image.mutable_pixels()) {
        const float alpha = p.a / 255.0f;
        auto apply = [&](uint8_t channel) {
            const float c = channel / 255.0f;
            float adjustedContrast;
            if (wContrast < 0.0f) adjustedContrast = std::max((c - 0.5f) * (1.0f + wContrast) + 0.5f, 0.0f);
            else adjustedContrast = std::min(c + (c - 0.5f) * wContrast, 1.0f);
            float adjustedBrightness;
            if (wBrightness < 0.0f) adjustedBrightness = adjustedContrast * (1.0f + wBrightness);
            else adjustedBrightness = adjustedContrast + wBrightness;
            return clamp_byte(std::min(adjustedBrightness, alpha) * 255.0f);
        };
        p.r=apply(p.r); p.g=apply(p.g); p.b=apply(p.b);
    }
}

Image crop(const Image& image,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    if(width==0||height==0||x>image.width()||y>image.height()||width>image.width()-x||height>image.height()-y)throw std::invalid_argument("crop rectangle outside image");
    Image out(width,height);for(uint32_t yy=0;yy<height;++yy)for(uint32_t xx=0;xx<width;++xx)out.at(xx,yy)=image.at(x+xx,y+yy);return out;
}
void flip_horizontal(Image& image){for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width()/2U;++x)std::swap(image.at(x,y),image.at(image.width()-1U-x,y));}
void flip_vertical(Image& image){for(uint32_t y=0;y<image.height()/2U;++y)for(uint32_t x=0;x<image.width();++x)std::swap(image.at(x,y),image.at(x,image.height()-1U-y));}
Image rotate_90_clockwise(const Image& image){Image out(image.height(),image.width());for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width();++x)out.at(image.height()-1U-y,x)=image.at(x,y);return out;}
Image rotate_90_counterclockwise(const Image& image){Image out(image.height(),image.width());for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width();++x)out.at(y,image.width()-1U-x)=image.at(x,y);return out;}
void grayscale(Image& image){for(auto& p:image.mutable_pixels()){const float y=0.2126f*p.r+0.7152f*p.g+0.0722f*p.b;const auto v=clamp_byte(y);p.r=p.g=p.b=v;}}
void sepia(Image& image){for(auto& p:image.mutable_pixels()){const float r=p.r,g=p.g,b=p.b;p.r=clamp_byte(0.393f*r+0.769f*g+0.189f*b);p.g=clamp_byte(0.349f*r+0.686f*g+0.168f*b);p.b=clamp_byte(0.272f*r+0.534f*g+0.131f*b);}}
void adjust_saturation(Image& image,float amount){if(!std::isfinite(amount))throw std::invalid_argument("saturation must be finite");amount=std::clamp(amount,-1.0f,1.0f);const float factor=1.0f+amount;for(auto& p:image.mutable_pixels()){const float gray=0.2126f*p.r+0.7152f*p.g+0.0722f*p.b;p.r=clamp_byte(gray+(p.r-gray)*factor);p.g=clamp_byte(gray+(p.g-gray)*factor);p.b=clamp_byte(gray+(p.b-gray)*factor);}}

}
