#include "pstouch/psd.hpp"
#include <cstdio>
#include <algorithm>
#include <array>
#include <limits>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace pstouch {
namespace {
void be16(std::ostream& o,uint16_t v){char b[2]={static_cast<char>(v>>8U),static_cast<char>(v)};o.write(b,2);}
void be32(std::ostream& o,uint32_t v){char b[4]={static_cast<char>(v>>24U),static_cast<char>(v>>16U),static_cast<char>(v>>8U),static_cast<char>(v)};o.write(b,4);}
bool replace_file(const std::filesystem::path& temporary,const std::filesystem::path& destination){
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return std::rename(temporary.c_str(),destination.c_str())==0;
#endif
}
}
void save_psd_flattened(const Image& image,const std::string& path){if(image.width()>30000||image.height()>30000)throw std::length_error("PSD dimensions exceed version-1 limits");const auto destination=std::filesystem::u8path(path);auto tmp=destination;tmp+=L".tmp";try{std::ofstream o(tmp,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("cannot create PSD file");o.write("8BPS",4);be16(o,1);char reserved[6]={0,0,0,0,0,0};o.write(reserved,6);be16(o,4);be32(o,image.height());be32(o,image.width());be16(o,8);be16(o,3);be32(o,0);be32(o,0);be32(o,0);be16(o,0);for(int channel=0;channel<4;++channel)for(const auto& p:image.pixels()){uint8_t v=channel==0?p.r:channel==1?p.g:channel==2?p.b:p.a;o.put(static_cast<char>(v));}o.flush();if(!o)throw std::runtime_error("PSD write failed");o.close();if(!replace_file(tmp,destination))throw std::runtime_error("could not finalize PSD file");}catch(...){std::error_code ignored;std::filesystem::remove(tmp,ignored);throw;}}

namespace {
uint16_t read_be16(std::istream& i){unsigned char b[2]{};i.read(reinterpret_cast<char*>(b),2);if(!i)throw std::runtime_error("truncated PSD");return static_cast<uint16_t>((static_cast<uint16_t>(b[0])<<8U)|b[1]);}
uint32_t read_be32(std::istream& i){unsigned char b[4]{};i.read(reinterpret_cast<char*>(b),4);if(!i)throw std::runtime_error("truncated PSD");return (static_cast<uint32_t>(b[0])<<24U)|(static_cast<uint32_t>(b[1])<<16U)|(static_cast<uint32_t>(b[2])<<8U)|b[3];}
void skip_bytes(std::istream& i,uint32_t n){i.seekg(static_cast<std::streamoff>(n),std::ios::cur);if(!i)throw std::runtime_error("truncated PSD section");}
void decode_packbits(const std::vector<uint8_t>& src,size_t& pos,size_t end,uint8_t* dst,size_t expected){size_t out=0;while(pos<end&&out<expected){int8_t n=static_cast<int8_t>(src[pos++]);if(n>=0){size_t count=static_cast<size_t>(n)+1;if(count>end-pos||count>expected-out)throw std::runtime_error("invalid PSD PackBits literal run");std::copy_n(src.data()+pos,count,dst+out);pos+=count;out+=count;}else if(n!=-128){size_t count=static_cast<size_t>(1-static_cast<int>(n));if(pos>=end||count>expected-out)throw std::runtime_error("invalid PSD PackBits repeat run");std::fill_n(dst+out,count,src[pos++]);out+=count;}}if(out!=expected)throw std::runtime_error("PSD PackBits row length mismatch");}
}
Image load_psd_flattened(const std::string& path){
 std::ifstream i(std::filesystem::u8path(path),std::ios::binary);if(!i)throw std::runtime_error("cannot open PSD file");char sig[4]{};i.read(sig,4);if(!i||std::string(sig,4)!="8BPS")throw std::runtime_error("not a PSD file");if(read_be16(i)!=1)throw std::runtime_error("only PSD version 1 is supported");char reserved[6];i.read(reserved,6);if(!i)throw std::runtime_error("truncated PSD header");uint16_t channels=read_be16(i);uint32_t height=read_be32(i),width=read_be32(i);uint16_t depth=read_be16(i),mode=read_be16(i);if(!width||!height||width>30000||height>30000||static_cast<uint64_t>(width)*height>100000000ULL)throw std::runtime_error("PSD dimensions exceed safety limits");if(depth!=8||mode!=3||(channels!=3&&channels!=4))throw std::runtime_error("PSD import supports only 8-bit RGB/RGBA");skip_bytes(i,read_be32(i));skip_bytes(i,read_be32(i));skip_bytes(i,read_be32(i));uint16_t compression=read_be16(i);if(compression>1)throw std::runtime_error("unsupported PSD compression");const size_t pixels=static_cast<size_t>(width)*height;std::vector<std::vector<uint8_t>> planes(channels,std::vector<uint8_t>(pixels));
 if(compression==0){for(auto& plane:planes){i.read(reinterpret_cast<char*>(plane.data()),static_cast<std::streamsize>(plane.size()));if(!i)throw std::runtime_error("truncated raw PSD pixel data");}}
 else {const size_t rows=static_cast<size_t>(height)*channels;std::vector<uint16_t> lengths(rows);for(auto& n:lengths)n=read_be16(i);const size_t max_row=static_cast<size_t>(width)+(static_cast<size_t>(width)+127U)/128U;size_t total=0;for(uint16_t n:lengths){if(n==0||n>max_row)throw std::runtime_error("invalid PSD RLE row length");if(total>static_cast<size_t>(channels)*height*max_row-static_cast<size_t>(n))throw std::runtime_error("PSD RLE data exceeds safety limit");total+=n;}std::vector<uint8_t> packed;packed.reserve(total);for(uint16_t n:lengths){std::vector<uint8_t> row(n);i.read(reinterpret_cast<char*>(row.data()),n);if(!i)throw std::runtime_error("truncated PSD RLE data");packed.insert(packed.end(),row.begin(),row.end());}size_t pos=0;for(size_t c=0;c<channels;++c)for(size_t y=0;y<height;++y){size_t len=lengths[c*height+y];size_t end=pos+len;if(end>packed.size())throw std::runtime_error("invalid PSD RLE table");decode_packbits(packed,pos,end,planes[c].data()+y*width,width);if(pos!=end)throw std::runtime_error("extra bytes in PSD PackBits row");}}
 Image out(width,height,{0,0,0,255});for(size_t n=0;n<pixels;++n){out.mutable_pixels()[n].r=planes[0][n];out.mutable_pixels()[n].g=planes[1][n];out.mutable_pixels()[n].b=planes[2][n];if(channels==4)out.mutable_pixels()[n].a=planes[3][n];}return out;
}

}
