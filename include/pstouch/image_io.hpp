#pragma once
#include "pstouch/image.hpp"
#include <string>
namespace pstouch {
// Supports PNG and JPEG based on the filename extension. JPEG is decoded as opaque RGBA.
Image load_image(const std::string& path);
void save_image(const Image& image, const std::string& path, int jpeg_quality = 92);
}
