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
void invert_colors(Image& image);
void posterize(Image& image, uint16_t levels); // 2..256 tonal levels per RGB channel

void gaussian_blur(Image& image, float sigma);
void sharpen(Image& image, float amount);
void edge_detect(Image& image);
void threshold(Image& image, uint8_t cutoff = 128);
void adjust_gamma(Image& image, float gamma);
void adjust_temperature(Image& image, float amount); // -1 cool, +1 warm
void vignette(Image& image, float amount); // 0..1
void pixelate(Image& image, uint32_t block_size);
void adjust_exposure(Image& image, float stops); // exposure compensation in EV, [-8, 8]
void adjust_hue(Image& image, float degrees); // hue rotation, [-180, 180]
void adjust_levels(Image& image, uint8_t black_point, uint8_t white_point, float gamma = 1.0f);
void auto_contrast(Image& image); // stretch each RGB channel to its observed range
void adjust_shadows_highlights(Image& image, float shadows, float highlights); // each in [-1, 1]
void color_balance(Image& image, float red, float green, float blue); // channel gains in [-1, 1]
void adjust_vibrance(Image& image, float amount); // selective saturation, amount in [-1, 1]
void equalize_luminance(Image& image); // histogram equalization on luminance, preserving alpha
}
