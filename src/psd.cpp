#include "pstouch/psd.hpp"
#include "pstouch/document.hpp"
#include <sstream>
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
void save_psd_flattened(const Image& image,const std::string& path){if(image.width()>30000||image.height()>30000)throw std::length_error("PSD dimensions exceed version-1 limits");const auto destination=std::filesystem::u8path(path);auto tmp=destination;tmp+=".tmp";try{std::ofstream o(tmp,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("cannot create PSD file");o.write("8BPS",4);be16(o,1);char reserved[6]={0,0,0,0,0,0};o.write(reserved,6);be16(o,4);be32(o,image.height());be32(o,image.width());be16(o,8);be16(o,3);be32(o,0);be32(o,0);be32(o,0);be16(o,0);for(int channel=0;channel<4;++channel)for(const auto& p:image.pixels()){uint8_t v=channel==0?p.r:channel==1?p.g:channel==2?p.b:p.a;o.put(static_cast<char>(v));}o.flush();if(!o)throw std::runtime_error("PSD write failed");o.close();if(!replace_file(tmp,destination))throw std::runtime_error("could not finalize PSD file");}catch(...){std::error_code ignored;std::filesystem::remove(tmp,ignored);throw;}}

namespace {
uint16_t read_be16(std::istream& i){unsigned char b[2]{};i.read(reinterpret_cast<char*>(b),2);if(!i)throw std::runtime_error("truncated PSD");return static_cast<uint16_t>((static_cast<uint16_t>(b[0])<<8U)|b[1]);}
uint32_t read_be32(std::istream& i){unsigned char b[4]{};i.read(reinterpret_cast<char*>(b),4);if(!i)throw std::runtime_error("truncated PSD");return (static_cast<uint32_t>(b[0])<<24U)|(static_cast<uint32_t>(b[1])<<16U)|(static_cast<uint32_t>(b[2])<<8U)|b[3];}
void skip_bytes(std::istream& i,uint32_t n){i.seekg(static_cast<std::streamoff>(n),std::ios::cur);if(!i)throw std::runtime_error("truncated PSD section");}
void decode_packbits(const std::vector<uint8_t>& src,size_t& pos,size_t end,uint8_t* dst,size_t expected){size_t out=0;while(pos<end&&out<expected){int8_t n=static_cast<int8_t>(src[pos++]);if(n>=0){size_t count=static_cast<size_t>(n)+1;if(count>end-pos||count>expected-out)throw std::runtime_error("invalid PSD PackBits literal run");std::copy_n(src.data()+pos,count,dst+out);pos+=count;out+=count;}else if(n!=-128){size_t count=static_cast<size_t>(1-static_cast<int>(n));if(pos>=end||count>expected-out)throw std::runtime_error("invalid PSD PackBits repeat run");std::fill_n(dst+out,count,src[pos++]);out+=count;}}if(out!=expected)throw std::runtime_error("PSD PackBits row length mismatch");}
}
void save_psd_layers(const Document& document,const std::string& path){
 const auto& layers=document.layers();
 if(layers.empty()||layers.size()>16000)throw std::invalid_argument("layered PSD requires 1..16000 layers");
 if(document.width()>30000||document.height()>30000)throw std::length_error("PSD dimensions exceed version-1 limits");
 auto put16=[](std::ostream& o,uint16_t v){be16(o,v);};
 auto put32=[](std::ostream& o,uint32_t v){be32(o,v);};
 std::ostringstream info(std::ios::out|std::ios::binary),records(std::ios::binary),channels(std::ios::binary);
 put16(info,static_cast<uint16_t>(layers.size()));
 for(auto it=layers.rbegin();it!=layers.rend();++it){
  const auto& l=*it;
  const int64_t right=static_cast<int64_t>(l.x)+l.image.width(),bottom=static_cast<int64_t>(l.y)+l.image.height();
  if(l.x<0||l.y<0||right>document.width()||bottom>document.height())throw std::invalid_argument("PSD layer bounds must fit inside canvas");
  if(l.name.size()>255)throw std::length_error("PSD layer names are limited to 255 UTF-8 bytes");
  put32(records,static_cast<uint32_t>(l.y));put32(records,static_cast<uint32_t>(l.x));put32(records,static_cast<uint32_t>(bottom));put32(records,static_cast<uint32_t>(right));
  put16(records,4);
  for(int16_t id : {-1,0,1,2}){put16(records,static_cast<uint16_t>(id));const uint64_t len=2ULL+static_cast<uint64_t>(l.image.width())*l.image.height();if(len>0xffffffffULL)throw std::length_error("PSD layer channel too large");put32(records,static_cast<uint32_t>(len));}
  records.write("8BIM",4);const char* blendKey="norm";switch(l.blend){case BlendMode::Normal:blendKey="norm";break;case BlendMode::Darken:blendKey="dark";break;case BlendMode::Multiply:blendKey="mul ";break;case BlendMode::Lighten:blendKey="lite";break;case BlendMode::Screen:blendKey="scrn";break;case BlendMode::Add:blendKey="lddg";break;case BlendMode::Overlay:blendKey="over";break;case BlendMode::Difference:blendKey="diff";break;case BlendMode::Subtract:throw std::invalid_argument("PSD export does not support the custom Subtract blend mode");}records.write(blendKey,4);records.put(static_cast<char>(l.opacity));records.put(0);records.put(static_cast<char>(l.visible?0:2));records.put(0);
  std::ostringstream extra(std::ios::out|std::ios::binary);put32(extra,0);put32(extra,0);
  const uint8_t nameLen=static_cast<uint8_t>(l.name.size());extra.put(static_cast<char>(nameLen));extra.write(l.name.data(),nameLen);
  const size_t nameBytes=1+l.name.size();for(size_t n=nameBytes;n%4;++n)extra.put(0);
  const auto extraData=extra.str();put32(records,static_cast<uint32_t>(extraData.size()));records.write(extraData.data(),static_cast<std::streamsize>(extraData.size()));
  for(int channel : {-1,0,1,2}){put16(channels,0);for(const auto& p:l.image.pixels()){const uint8_t v=channel==-1?p.a:channel==0?p.r:channel==1?p.g:p.b;channels.put(static_cast<char>(v));}}
 }
 const auto rec=records.str(),ch=channels.str();info.write(rec.data(),static_cast<std::streamsize>(rec.size()));info.write(ch.data(),static_cast<std::streamsize>(ch.size()));
 if(!info)throw std::runtime_error("could not assemble PSD layer information");
 std::ostringstream layerMask(std::ios::out|std::ios::binary);const auto li=info.str();put32(layerMask,static_cast<uint32_t>(li.size()));layerMask.write(li.data(),static_cast<std::streamsize>(li.size()));put32(layerMask,0);
 const auto lm=layerMask.str();if(lm.size()>0xffffffffULL)throw std::length_error("PSD layer information too large");
 const Image merged=document.composite();const auto destination=std::filesystem::u8path(path);auto tmp=destination;tmp+=".tmp";
 try{
  std::ofstream o(tmp,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("cannot create PSD file");
  o.write("8BPS",4);put16(o,1);char reserved[6]={};o.write(reserved,6);put16(o,4);put32(o,document.height());put32(o,document.width());put16(o,8);put16(o,3);put32(o,0);put32(o,0);put32(o,static_cast<uint32_t>(lm.size()));o.write(lm.data(),static_cast<std::streamsize>(lm.size()));put16(o,0);
  for(int channel=0;channel<4;++channel)for(const auto& p:merged.pixels())o.put(static_cast<char>(channel==0?p.r:channel==1?p.g:channel==2?p.b:p.a));
  o.flush();if(!o)throw std::runtime_error("PSD write failed");o.close();if(!replace_file(tmp,destination))throw std::runtime_error("could not finalize PSD file");
 }catch(...){std::error_code ignored;std::filesystem::remove(tmp,ignored);throw;}
}


Document load_psd_layers(const std::string& path){
 std::ifstream i(std::filesystem::u8path(path),std::ios::binary);
 if(!i)throw std::runtime_error("cannot open PSD file");
 char sig[4]{};i.read(sig,4);
 if(!i||std::string(sig,4)!="8BPS")throw std::runtime_error("not a PSD file");
 if(read_be16(i)!=1)throw std::runtime_error("only PSD version 1 is supported");
 char reserved[6]{};i.read(reserved,6);if(!i)throw std::runtime_error("truncated PSD header");
 const uint16_t channels=read_be16(i);const uint32_t height=read_be32(i),width=read_be32(i);
 const uint16_t depth=read_be16(i),mode=read_be16(i);
 if(!width||!height||width>30000||height>30000||static_cast<uint64_t>(width)*height>100000000ULL)throw std::runtime_error("PSD dimensions exceed safety limits");
 if(depth!=8||mode!=3||channels<3||channels>4)throw std::runtime_error("layered PSD import supports only 8-bit RGB/RGBA");
 skip_bytes(i,read_be32(i));skip_bytes(i,read_be32(i));
 const uint32_t outerLength=read_be32(i);
 const auto outerStart=i.tellg();
 if(outerLength<4)throw std::runtime_error("invalid PSD layer/mask section");
 const uint32_t infoLength=read_be32(i);
 if(infoLength>outerLength-4)throw std::runtime_error("invalid PSD layer info length");
 const auto infoStart=i.tellg();
 const int16_t signedCount=static_cast<int16_t>(read_be16(i));
 const uint32_t count=static_cast<uint32_t>(signedCount<0?-static_cast<int32_t>(signedCount):signedCount);
 if(count==0||count>512)throw std::runtime_error("layered PSD import requires 1..512 layers");
 struct Channel { int16_t id{}; uint32_t length{}; };
 struct Record { int32_t top{},left{},bottom{},right{};uint8_t opacity{255};bool visible{true};BlendMode blend{BlendMode::Normal};std::string name;std::vector<Channel> channels;Image image{1,1,{0,0,0,0}}; };
 std::vector<Record> records;records.reserve(count);
 auto read_s32=[&](){return static_cast<int32_t>(read_be32(i));};
 auto read_fourcc=[&](){char b[4]{};i.read(b,4);if(!i)throw std::runtime_error("truncated PSD layer record");return std::string(b,4);};
 for(uint32_t n=0;n<count;++n){
  Record r;r.top=read_s32();r.left=read_s32();r.bottom=read_s32();r.right=read_s32();
  const uint16_t cc=read_be16(i);if(cc==0||cc>16)throw std::runtime_error("invalid PSD layer channel count");
  for(uint16_t c=0;c<cc;++c){Channel ch;ch.id=static_cast<int16_t>(read_be16(i));ch.length=read_be32(i);if(ch.length<2)throw std::runtime_error("invalid PSD channel data length");r.channels.push_back(ch);}
  if(read_fourcc()!="8BIM")throw std::runtime_error("unsupported PSD layer blend signature");
  const std::string key=read_fourcc();
  char meta[4]{};i.read(meta,4);if(!i)throw std::runtime_error("truncated PSD layer blend metadata");
  r.opacity=static_cast<uint8_t>(meta[0]);r.visible=(static_cast<uint8_t>(meta[2])&2U)==0;
  if(key=="norm")r.blend=BlendMode::Normal;else if(key=="dark")r.blend=BlendMode::Darken;else if(key=="mul ")r.blend=BlendMode::Multiply;else if(key=="lite")r.blend=BlendMode::Lighten;else if(key=="scrn")r.blend=BlendMode::Screen;else if(key=="lddg")r.blend=BlendMode::Add;else if(key=="over")r.blend=BlendMode::Overlay;else if(key=="diff")r.blend=BlendMode::Difference;else throw std::runtime_error("unsupported PSD layer blend mode");
  const uint32_t extraLength=read_be32(i);const auto extraStart=i.tellg();
  if(extraLength>infoLength)throw std::runtime_error("invalid PSD layer extra data length");
  const uint32_t maskLength=read_be32(i);skip_bytes(i,maskLength);
  const uint32_t blendRangeLength=read_be32(i);skip_bytes(i,blendRangeLength);
  const uint8_t nameLength=static_cast<uint8_t>(i.get());if(!i)throw std::runtime_error("truncated PSD layer name");
  r.name.resize(nameLength);i.read(r.name.data(),nameLength);if(!i)throw std::runtime_error("truncated PSD layer name");
  const size_t consumed=4ULL+maskLength+4ULL+blendRangeLength+1ULL+nameLength;
  const size_t padding=(4-(1+nameLength)%4)%4;skip_bytes(i,static_cast<uint32_t>(padding));
  const auto now=i.tellg();const auto used=static_cast<uint64_t>(now-extraStart);
  if(used>extraLength)throw std::runtime_error("invalid PSD layer extra data");
  if(used<extraLength)skip_bytes(i,static_cast<uint32_t>(extraLength-used));
  (void)consumed;
  const int64_t rw=static_cast<int64_t>(r.right)-r.left,rh=static_cast<int64_t>(r.bottom)-r.top;
  if(rw<=0||rh<=0||rw>width||rh>height||r.left<0||r.top<0||r.right>static_cast<int32_t>(width)||r.bottom>static_cast<int32_t>(height)||static_cast<uint64_t>(rw)*static_cast<uint64_t>(rh)>100000000ULL)throw std::runtime_error("PSD layer bounds are invalid or outside canvas");
  records.push_back(std::move(r));
 }
 const auto afterRecords=i.tellg();
 const auto consumedInfo=static_cast<uint64_t>(afterRecords-infoStart);
 if(consumedInfo>infoLength)throw std::runtime_error("PSD layer records exceed section");
 for(auto& r:records){
  const uint32_t rw=static_cast<uint32_t>(r.right-r.left),rh=static_cast<uint32_t>(r.bottom-r.top);
  r.image=Image(rw,rh,{0,0,0,0});
  std::vector<bool> seen(4,false);
  for(const auto& ch:r.channels){
   const uint16_t compression=read_be16(i);
   const uint64_t expected=static_cast<uint64_t>(rw)*rh;
   int plane=-1;if(ch.id==-1)plane=3;else if(ch.id==0)plane=0;else if(ch.id==1)plane=1;else if(ch.id==2)plane=2;
   const uint64_t remaining=static_cast<uint64_t>(ch.length)-2;
   std::vector<uint8_t> decoded;
   if(compression==0){
    if(remaining!=expected)throw std::runtime_error("PSD raw layer channel length does not match bounds");
    if(plane>=0){decoded.resize(static_cast<size_t>(expected));i.read(reinterpret_cast<char*>(decoded.data()),static_cast<std::streamsize>(decoded.size()));if(!i)throw std::runtime_error("truncated PSD layer pixels");}
    else skip_bytes(i,static_cast<uint32_t>(remaining));
   }else if(compression==1){
    const uint64_t tableBytes=static_cast<uint64_t>(rh)*2;
    if(remaining<tableBytes||remaining>0xffffffffULL)throw std::runtime_error("invalid PSD RLE layer channel length");
    std::vector<uint16_t> rowLengths(rh);uint64_t packedLength=0;
    for(auto& length:rowLengths){length=read_be16(i);packedLength+=length;}
    if(packedLength!=remaining-tableBytes)throw std::runtime_error("PSD RLE layer channel length mismatch");
    std::vector<uint8_t> packed(static_cast<size_t>(packedLength));i.read(reinterpret_cast<char*>(packed.data()),static_cast<std::streamsize>(packed.size()));if(!i)throw std::runtime_error("truncated PSD RLE layer channel");
    if(plane>=0){decoded.resize(static_cast<size_t>(expected));size_t pos=0;for(uint32_t y=0;y<rh;++y){const size_t end=pos+rowLengths[y];if(end>packed.size())throw std::runtime_error("invalid PSD RLE layer row table");decode_packbits(packed,pos,end,decoded.data()+static_cast<size_t>(y)*rw,rw);if(pos!=end)throw std::runtime_error("extra bytes in PSD RLE layer row");}}
   }else throw std::runtime_error("unsupported PSD layer channel compression");
   if(plane>=0){
    if(seen[static_cast<size_t>(plane)])throw std::runtime_error("duplicate PSD layer channel");
    seen[static_cast<size_t>(plane)]=true;
    for(size_t px=0;px<decoded.size();++px){auto& p=r.image.mutable_pixels()[px];const auto value=decoded[px];if(plane==0)p.r=value;else if(plane==1)p.g=value;else if(plane==2)p.b=value;else p.a=value;}
   }
  }
  if(!seen[0]||!seen[1]||!seen[2])throw std::runtime_error("PSD layer is missing RGB channels");
  if(!seen[3])for(auto& p:r.image.mutable_pixels())p.a=255;
 }
 const auto afterChannels=i.tellg();
 if(static_cast<uint64_t>(afterChannels-infoStart)>infoLength)throw std::runtime_error("PSD layer channel data exceeds section");
 i.seekg(infoStart+static_cast<std::streamoff>(infoLength));
 const auto endInfo=i.tellg();
 if(endInfo<0)throw std::runtime_error("invalid PSD layer info boundary");
 const uint64_t usedOuter=static_cast<uint64_t>(endInfo-outerStart);
 if(usedOuter+4>outerLength)throw std::runtime_error("invalid PSD layer/mask boundary");
 skip_bytes(i,static_cast<uint32_t>(outerLength-usedOuter-4));
 Document doc(width,height,"Imported PSD");
 for(auto it=records.rbegin();it!=records.rend();++it){Layer layer(it->name,std::move(it->image));layer.x=it->left;layer.y=it->top;layer.opacity=it->opacity;layer.visible=it->visible;layer.blend=it->blend;doc.add_layer(std::move(layer));}
 return doc;
}

Image load_psd_flattened(const std::string& path){
 std::ifstream i(std::filesystem::u8path(path),std::ios::binary);if(!i)throw std::runtime_error("cannot open PSD file");char sig[4]{};i.read(sig,4);if(!i||std::string(sig,4)!="8BPS")throw std::runtime_error("not a PSD file");if(read_be16(i)!=1)throw std::runtime_error("only PSD version 1 is supported");char reserved[6];i.read(reserved,6);if(!i)throw std::runtime_error("truncated PSD header");uint16_t channels=read_be16(i);uint32_t height=read_be32(i),width=read_be32(i);uint16_t depth=read_be16(i),mode=read_be16(i);if(!width||!height||width>30000||height>30000||static_cast<uint64_t>(width)*height>100000000ULL)throw std::runtime_error("PSD dimensions exceed safety limits");if(depth!=8||mode!=3||(channels!=3&&channels!=4))throw std::runtime_error("PSD import supports only 8-bit RGB/RGBA");skip_bytes(i,read_be32(i));skip_bytes(i,read_be32(i));skip_bytes(i,read_be32(i));uint16_t compression=read_be16(i);if(compression>1)throw std::runtime_error("unsupported PSD compression");const size_t pixels=static_cast<size_t>(width)*height;std::vector<std::vector<uint8_t>> planes(channels,std::vector<uint8_t>(pixels));
 if(compression==0){for(auto& plane:planes){i.read(reinterpret_cast<char*>(plane.data()),static_cast<std::streamsize>(plane.size()));if(!i)throw std::runtime_error("truncated raw PSD pixel data");}}
 else {const size_t rows=static_cast<size_t>(height)*channels;std::vector<uint16_t> lengths(rows);for(auto& n:lengths)n=read_be16(i);const size_t max_row=static_cast<size_t>(width)+(static_cast<size_t>(width)+127U)/128U;size_t total=0;for(uint16_t n:lengths){if(n==0||n>max_row)throw std::runtime_error("invalid PSD RLE row length");if(total>static_cast<size_t>(channels)*height*max_row-static_cast<size_t>(n))throw std::runtime_error("PSD RLE data exceeds safety limit");total+=n;}std::vector<uint8_t> packed;packed.reserve(total);for(uint16_t n:lengths){std::vector<uint8_t> row(n);i.read(reinterpret_cast<char*>(row.data()),n);if(!i)throw std::runtime_error("truncated PSD RLE data");packed.insert(packed.end(),row.begin(),row.end());}size_t pos=0;for(size_t c=0;c<channels;++c)for(size_t y=0;y<height;++y){size_t len=lengths[c*height+y];size_t end=pos+len;if(end>packed.size())throw std::runtime_error("invalid PSD RLE table");decode_packbits(packed,pos,end,planes[c].data()+y*width,width);if(pos!=end)throw std::runtime_error("extra bytes in PSD PackBits row");}}
 Image out(width,height,{0,0,0,255});for(size_t n=0;n<pixels;++n){out.mutable_pixels()[n].r=planes[0][n];out.mutable_pixels()[n].g=planes[1][n];out.mutable_pixels()[n].b=planes[2][n];if(channels==4)out.mutable_pixels()[n].a=planes[3][n];}return out;
}

}
