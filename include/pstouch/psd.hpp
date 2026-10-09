#pragma once
#include "pstouch/image.hpp"
#include <string>
namespace pstouch {
// Writes a flattened RGB PSD v1 with alpha; layer records are not emitted.
void save_psd_flattened(const Image& image, const std::string& path);
// Reads flattened PSD v1 RGB documents (8-bit, 3 or 4 channels; raw or PackBits RLE).
// Layered PSD files are currently flattened on import; unsupported modes are rejected.
Image load_psd_flattened(const std::string& path);
}
