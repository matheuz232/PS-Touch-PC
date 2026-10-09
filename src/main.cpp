#include "pstouch/image_io.hpp"
#include "pstouch/psd.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
namespace {
std::string extension(std::string path) {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}
pstouch::Image load(const std::string& path) {
    return extension(path) == ".psd" ? pstouch::load_psd_flattened(path) : pstouch::load_image(path);
}
void save(const pstouch::Image& image, const std::string& path) {
    if (extension(path) == ".psd") pstouch::save_psd_flattened(image, path);
    else pstouch::save_image(image, path);
}
}
int main(int argc,char** argv) {
    if(argc<3 || argc>5) { std::cerr<<"Usage: pstouch-image <input.png|jpg|psd> <output.png|jpg|psd> [brightness -1..1] [contrast -1..1]\n       pstouch-image <input> <output> --op grayscale|sepia|saturation|flip-h|flip-v|rotate-cw|rotate-ccw [amount]\n"; return 2; }
    try {
        auto image=load(argv[1]);
        if(argc>=4 && std::string(argv[3])=="--op") {
            if(argc<5 || argc>6) throw std::invalid_argument("--op requires an operation and optional amount");
            const std::string op=argv[4]; const float amount=argc==6?std::stof(argv[5]):0.0f;
            if(op=="grayscale") pstouch::grayscale(image);
            else if(op=="sepia") pstouch::sepia(image);
            else if(op=="saturation") pstouch::adjust_saturation(image,amount);
            else if(op=="flip-h") pstouch::flip_horizontal(image);
            else if(op=="flip-v") pstouch::flip_vertical(image);
            else if(op=="rotate-cw") image=pstouch::rotate_90_clockwise(image);
            else if(op=="rotate-ccw") image=pstouch::rotate_90_counterclockwise(image);
            else throw std::invalid_argument("unknown image operation");
        } else {
            const float brightness=argc>=4?std::stof(argv[3]):0.0f;
            const float contrast=argc>=5?std::stof(argv[4]):0.0f;
            pstouch::brightness_contrast(image,brightness,contrast);
        }
        save(image,argv[2]);
        std::cout<<"Saved "<<argv[2]<<" ("<<image.width()<<"x"<<image.height()<<")\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"Error: "<<e.what()<<"\n"; return 1; }
}
