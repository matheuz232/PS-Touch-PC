#include "pstouch/image_io.hpp"
#include <png.h>
#include <jpeglib.h>
#include <csetjmp>
#include <cstdio>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace pstouch {
namespace {
std::string extension(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}
struct JpegError { jpeg_error_mgr base; std::jmp_buf jump; char message[JMSG_LENGTH_MAX]{}; };
void jpeg_error_exit(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<JpegError*>(cinfo->err);
    (*cinfo->err->format_message)(cinfo, err->message);
    std::longjmp(err->jump, 1);
}
Image load_png(const std::string& path) {
    png_image img{}; img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&img, path.c_str()))
        throw std::runtime_error("PNG read failed: " + std::string(img.message));
    img.format = PNG_FORMAT_RGBA;
    if (img.width == 0 || img.height == 0 || static_cast<uint64_t>(img.width) * img.height > 100000000ULL) {
        png_image_free(&img); throw std::runtime_error("PNG dimensions exceed safety limit");
    }
    std::vector<png_byte> bytes(PNG_IMAGE_SIZE(img));
    if (!png_image_finish_read(&img, nullptr, bytes.data(), 0, nullptr)) {
        const std::string msg = img.message; png_image_free(&img);
        throw std::runtime_error("PNG decode failed: " + msg);
    }
    Image out(img.width, img.height);
    auto& dst = out.mutable_pixels();
    for (size_t i = 0; i < dst.size(); ++i) dst[i] = {bytes[i*4],bytes[i*4+1],bytes[i*4+2],bytes[i*4+3]};
    png_image_free(&img); return out;
}
void save_png(const Image& image, const std::string& path) {
    png_image img{}; img.version = PNG_IMAGE_VERSION; img.width = image.width(); img.height = image.height(); img.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> bytes(image.pixels().size()*4);
    for (size_t i = 0; i < image.pixels().size(); ++i) {
        const auto& p=image.pixels()[i]; bytes[i*4]=p.r; bytes[i*4+1]=p.g; bytes[i*4+2]=p.b; bytes[i*4+3]=p.a;
    }
    if (!png_image_write_to_file(&img, path.c_str(), 0, bytes.data(), 0, nullptr))
        throw std::runtime_error("PNG write failed: " + std::string(img.message));
}
Image load_jpeg(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb"); if (!file) throw std::runtime_error("cannot open JPEG: " + path);
    jpeg_decompress_struct cinfo{}; JpegError err{}; cinfo.err=jpeg_std_error(&err.base); err.base.error_exit=jpeg_error_exit;
    if (setjmp(err.jump)) { jpeg_destroy_decompress(&cinfo); std::fclose(file); throw std::runtime_error("JPEG decode failed: " + std::string(err.message)); }
    jpeg_create_decompress(&cinfo); jpeg_stdio_src(&cinfo,file); jpeg_read_header(&cinfo,TRUE);
    cinfo.out_color_space=JCS_RGB; jpeg_start_decompress(&cinfo);
    if (!cinfo.output_width || !cinfo.output_height || static_cast<uint64_t>(cinfo.output_width)*cinfo.output_height>100000000ULL) {
        jpeg_destroy_decompress(&cinfo); std::fclose(file); throw std::runtime_error("JPEG dimensions exceed safety limit");
    }
    Image out(cinfo.output_width,cinfo.output_height); std::vector<JSAMPLE> row(static_cast<size_t>(cinfo.output_width)*3);
    while (cinfo.output_scanline<cinfo.output_height) {
        JSAMPROW rows[1]={row.data()}; jpeg_read_scanlines(&cinfo,rows,1); const uint32_t y=cinfo.output_scanline-1;
        for (uint32_t x=0;x<cinfo.output_width;++x) out.at(x,y)={row[x*3],row[x*3+1],row[x*3+2],255};
    }
    jpeg_finish_decompress(&cinfo); jpeg_destroy_decompress(&cinfo); std::fclose(file); return out;
}
void save_jpeg(const Image& image, const std::string& path, int quality) {
    FILE* file=std::fopen(path.c_str(),"wb"); if (!file) throw std::runtime_error("cannot create JPEG: " + path);
    jpeg_compress_struct cinfo{}; JpegError err{}; cinfo.err=jpeg_std_error(&err.base); err.base.error_exit=jpeg_error_exit;
    if (setjmp(err.jump)) { jpeg_destroy_compress(&cinfo); std::fclose(file); throw std::runtime_error("JPEG encode failed: " + std::string(err.message)); }
    jpeg_create_compress(&cinfo); jpeg_stdio_dest(&cinfo,file); cinfo.image_width=image.width(); cinfo.image_height=image.height();
    cinfo.input_components=3; cinfo.in_color_space=JCS_RGB; jpeg_set_defaults(&cinfo); jpeg_set_quality(&cinfo,quality,TRUE); jpeg_start_compress(&cinfo,TRUE);
    std::vector<JSAMPLE> row(static_cast<size_t>(image.width())*3);
    while (cinfo.next_scanline<cinfo.image_height) {
        const uint32_t y=cinfo.next_scanline;
        for (uint32_t x=0;x<image.width();++x) { const auto p=image.at(x,y); const float a=p.a/255.0f; const size_t i=static_cast<size_t>(x)*3; row[i]=static_cast<JSAMPLE>(p.r*a+255.0f*(1.0f-a)); row[i+1]=static_cast<JSAMPLE>(p.g*a+255.0f*(1.0f-a)); row[i+2]=static_cast<JSAMPLE>(p.b*a+255.0f*(1.0f-a)); }
        JSAMPROW rows[1]={row.data()}; jpeg_write_scanlines(&cinfo,rows,1);
    }
    jpeg_finish_compress(&cinfo); jpeg_destroy_compress(&cinfo); std::fclose(file);
}
}
Image load_image(const std::string& path) { const auto ext=extension(path); if(ext=="png") return load_png(path); if(ext=="jpg"||ext=="jpeg") return load_jpeg(path); throw std::invalid_argument("supported input formats: PNG, JPG, JPEG"); }
void save_image(const Image& image, const std::string& path, int quality) { const auto ext=extension(path); if(ext=="png") return save_png(image,path); if(ext=="jpg"||ext=="jpeg") { if(quality<1||quality>100) throw std::invalid_argument("JPEG quality must be 1..100"); return save_jpeg(image,path,quality); } throw std::invalid_argument("supported output formats: PNG, JPG, JPEG"); }
}
