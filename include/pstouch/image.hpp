#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace pstouch {
struct Pixel { uint8_t r{}, g{}, b{}, a{255}; };
class Image {
public:
    Image(uint32_t width, uint32_t height, Pixel fill = {});
    uint32_t width() const noexcept { return width_; }
    uint32_t height() const noexcept { return height_; }
    Pixel& at(uint32_t x, uint32_t y);
    const Pixel& at(uint32_t x, uint32_t y) const;
    const std::vector<Pixel>& pixels() const noexcept { return pixels_; }
    std::vector<Pixel>& mutable_pixels() noexcept { return pixels_; }
private:
    uint32_t width_, height_;
    std::vector<Pixel> pixels_;
};
void premultiply_alpha(Image& image);
void unpremultiply_alpha(Image& image);
void source_over(Image& destination, const Image& source, int32_t x, int32_t y);
Image resample_bilinear(const Image& source, uint32_t width, uint32_t height);
void brightness_contrast(Image& image, float brightness, float contrast);
Image crop(const Image& image, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void flip_horizontal(Image& image);
void flip_vertical(Image& image);
Image rotate_90_clockwise(const Image& image);
Image rotate_90_counterclockwise(const Image& image);
void grayscale(Image& image);
void sepia(Image& image);
void adjust_saturation(Image& image, float amount); // -1 removes saturation, +1 doubles it
}
