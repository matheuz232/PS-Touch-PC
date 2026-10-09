#include "pstouch/document.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace pstouch {
namespace {
constexpr char kMagic[8]={'P','T','D','O','C','0','0','1'};
constexpr uint32_t kMaxLayers=512, kMaxString=4096;
void write_bytes(std::ostream& o,const void* p,size_t n){o.write(static_cast<const char*>(p),static_cast<std::streamsize>(n));if(!o)throw std::runtime_error("project write failed");}
void read_bytes(std::istream& i,void* p,size_t n){i.read(static_cast<char*>(p),static_cast<std::streamsize>(n));if(!i)throw std::runtime_error("invalid or truncated project file");}
bool replace_file(const std::string& temporary,const std::string& destination){
#ifdef _WIN32
    return MoveFileExA(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return std::rename(temporary.c_str(),destination.c_str())==0;
#endif
}
template<class T> void write_le(std::ostream& o,T v){static_assert(std::is_integral<T>::value,"integer only");for(size_t n=0;n<sizeof(T);++n){const uint8_t b=static_cast<uint8_t>((static_cast<uint64_t>(v)>>(n*8U))&255U);write_bytes(o,&b,1);}}
template<class T> T read_le(std::istream& i){static_assert(std::is_integral<T>::value,"integer only");uint64_t v=0;for(size_t n=0;n<sizeof(T);++n){uint8_t b{};read_bytes(i,&b,1);v|=static_cast<uint64_t>(b)<<(n*8U);}return static_cast<T>(v);}
void write_string(std::ostream& o,const std::string& s){if(s.size()>kMaxString)throw std::length_error("project string too long");write_le<uint32_t>(o,static_cast<uint32_t>(s.size()));write_bytes(o,s.data(),s.size());}
std::string read_string(std::istream& i){auto n=read_le<uint32_t>(i);if(n>kMaxString)throw std::runtime_error("project string too long");std::string s(n,'\0');read_bytes(i,s.data(),n);return s;}
uint8_t clamp(float v){return static_cast<uint8_t>(std::clamp(std::lround(v),0L,255L));}
float blend_channel(float s,float d,BlendMode mode){switch(mode){case BlendMode::Darken:return std::min(s,d);case BlendMode::Multiply:return s*d;case BlendMode::Lighten:return std::max(s,d);case BlendMode::Screen:return 1.0f-(1.0f-s)*(1.0f-d);case BlendMode::Add:return std::min(1.0f,s+d);case BlendMode::Overlay:return d<0.5f?2.0f*s*d:1.0f-2.0f*(1.0f-s)*(1.0f-d);case BlendMode::Difference:return std::abs(d-s);case BlendMode::Subtract:return std::max(0.0f,d-s);default:return s;}}
void composite_layer(Image& dst,const Layer& layer){if(!layer.visible||layer.opacity==0)return;for(uint32_t sy=0;sy<layer.image.height();++sy)for(uint32_t sx=0;sx<layer.image.width();++sx){int64_t dx=static_cast<int64_t>(layer.x)+sx,dy=static_cast<int64_t>(layer.y)+sy;if(dx<0||dy<0||dx>=dst.width()||dy>=dst.height())continue;const Pixel s=layer.image.at(sx,sy);Pixel& d=dst.at(static_cast<uint32_t>(dx),static_cast<uint32_t>(dy));const float sa=(s.a/255.0f)*(layer.opacity/255.0f), da=d.a/255.0f, oa=sa+da*(1.0f-sa);if(oa<=0){d={0,0,0,0};continue;}auto ch=[&](uint8_t sc,uint8_t dc){float sf=sc/255.0f,df=dc/255.0f,b=blend_channel(sf,df,layer.blend);return clamp(((1.0f-sa)*df*da+sa*((1.0f-da)*sf+da*b))/oa*255.0f);};d.r=ch(s.r,d.r);d.g=ch(s.g,d.g);d.b=ch(s.b,d.b);d.a=clamp(oa*255.0f);}}
}
Layer::Layer(std::string n,Image im):name(std::move(n)),image(std::move(im)){}
Document::Document(uint32_t w,uint32_t h,std::string n):width_(w),height_(h),name_(std::move(n)){if(!w||!h||static_cast<uint64_t>(w)*h>100000000ULL)throw std::invalid_argument("invalid document dimensions");}
void Document::set_name(std::string n){if(n.size()>kMaxString)throw std::length_error("document name too long");name_=std::move(n);}
size_t Document::add_layer(Layer l){if(layers_.size()>=kMaxLayers)throw std::length_error("layer limit reached");if(static_cast<unsigned>(l.blend)>static_cast<unsigned>(BlendMode::Subtract))throw std::invalid_argument("invalid blend mode");layers_.push_back(std::move(l));return layers_.size()-1;}
void Document::remove_layer(size_t i){if(i>=layers_.size())throw std::out_of_range("layer index");layers_.erase(layers_.begin()+static_cast<std::ptrdiff_t>(i));}
void Document::move_layer(size_t from,size_t to){if(from>=layers_.size()||to>=layers_.size())throw std::out_of_range("layer index");if(from==to)return;Layer l=std::move(layers_[from]);layers_.erase(layers_.begin()+static_cast<std::ptrdiff_t>(from));layers_.insert(layers_.begin()+static_cast<std::ptrdiff_t>(to),std::move(l));}
void Document::rename_layer(size_t i,std::string n){if(i>=layers_.size())throw std::out_of_range("layer index");if(n.size()>kMaxString)throw std::length_error("layer name too long");layers_[i].name=std::move(n);}
void Document::set_layer_visibility(size_t i,bool v){if(i>=layers_.size())throw std::out_of_range("layer index");layers_[i].visible=v;}
void Document::set_layer_opacity(size_t i,uint8_t v){if(i>=layers_.size())throw std::out_of_range("layer index");layers_[i].opacity=v;}
size_t Document::duplicate_layer(size_t i){if(i>=layers_.size())throw std::out_of_range("layer index");if(layers_.size()>=kMaxLayers)throw std::length_error("layer limit reached");Layer copy=layers_[i];copy.name += " copy";layers_.insert(layers_.begin()+static_cast<std::ptrdiff_t>(i+1),std::move(copy));return i+1;}
Image Document::composite()const{Image out(width_,height_,{0,0,0,0});for(const auto& layer:layers_)composite_layer(out,layer);return out;}
void Document::save(const std::string& path)const{if(layers_.size()>kMaxLayers)throw std::length_error("too many layers");const std::string tmp=path+".tmp";try{std::ofstream o(tmp,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("cannot create project file");write_bytes(o,kMagic,sizeof(kMagic));write_le<uint32_t>(o,width_);write_le<uint32_t>(o,height_);write_string(o,name_);write_le<uint32_t>(o,static_cast<uint32_t>(layers_.size()));for(const auto& l:layers_){write_string(o,l.name);write_le<int32_t>(o,l.x);write_le<int32_t>(o,l.y);write_le<uint8_t>(o,l.opacity);write_le<uint8_t>(o,l.visible?1:0);write_le<uint8_t>(o,static_cast<uint8_t>(l.blend));write_le<uint8_t>(o,0);write_le<uint32_t>(o,l.image.width());write_le<uint32_t>(o,l.image.height());for(const auto& p:l.image.pixels()){write_le<uint8_t>(o,p.r);write_le<uint8_t>(o,p.g);write_le<uint8_t>(o,p.b);write_le<uint8_t>(o,p.a);}}o.flush();if(!o)throw std::runtime_error("project flush failed");o.close();if(!replace_file(tmp,path))throw std::runtime_error("could not finalize project file");}catch(...){std::remove(tmp.c_str());throw;}}
Document Document::load(const std::string& path){std::ifstream i(path,std::ios::binary);if(!i)throw std::runtime_error("cannot open project file");char magic[8];read_bytes(i,magic,sizeof(magic));if(std::memcmp(magic,kMagic,sizeof(magic))!=0)throw std::runtime_error("not a PTDOC v1 project");uint32_t w=read_le<uint32_t>(i),h=read_le<uint32_t>(i);Document d(w,h,read_string(i));uint32_t count=read_le<uint32_t>(i);if(count>kMaxLayers)throw std::runtime_error("project has too many layers");for(uint32_t n=0;n<count;++n){std::string name=read_string(i);int32_t x=read_le<int32_t>(i),y=read_le<int32_t>(i);uint8_t opacity=read_le<uint8_t>(i),visible=read_le<uint8_t>(i),blend=read_le<uint8_t>(i);(void)read_le<uint8_t>(i);uint32_t lw=read_le<uint32_t>(i),lh=read_le<uint32_t>(i);if(!lw||!lh||static_cast<uint64_t>(lw)*lh>100000000ULL||blend>static_cast<uint8_t>(BlendMode::Subtract)||visible>1)throw std::runtime_error("invalid layer metadata");Image image(lw,lh);for(auto& p:image.mutable_pixels()){p.r=read_le<uint8_t>(i);p.g=read_le<uint8_t>(i);p.b=read_le<uint8_t>(i);p.a=read_le<uint8_t>(i);}Layer layer(std::move(name),std::move(image));layer.x=x;layer.y=y;layer.opacity=opacity;layer.visible=visible!=0;layer.blend=static_cast<BlendMode>(blend);d.add_layer(std::move(layer));}char extra;if(i.read(&extra,1))throw std::runtime_error("unexpected trailing data in project");return d;}
Document::State Document::snapshot(std::string label)const{return {std::move(label),width_,height_,name_,layers_};}
void Document::restore(const State&s){width_=s.width;height_=s.height;name_=s.name;layers_=s.layers;}
void Document::checkpoint(std::string label){if(history_cursor_<history_.size())history_.erase(history_.begin()+static_cast<std::ptrdiff_t>(history_cursor_),history_.end());history_.push_back(snapshot(std::move(label)));if(history_.size()>kMaxHistory)history_.erase(history_.begin());history_cursor_=history_.size();}
bool Document::undo(){if(history_cursor_<=1)return false;--history_cursor_;restore(history_[history_cursor_-1]);return true;}
bool Document::redo(){if(history_cursor_>=history_.size())return false;restore(history_[history_cursor_]);++history_cursor_;return true;}
bool Document::can_undo()const noexcept{return history_cursor_>1;}bool Document::can_redo()const noexcept{return history_cursor_<history_.size();}
const std::string& Document::undo_label()const noexcept{static const std::string empty;return can_undo()?history_[history_cursor_-1].label:empty;}
const std::string& Document::redo_label()const noexcept{static const std::string empty;return can_redo()?history_[history_cursor_].label:empty;}
}
