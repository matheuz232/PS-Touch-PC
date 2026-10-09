#include "pstouch/document.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
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
bool replace_file(const std::filesystem::path& temporary,const std::filesystem::path& destination){
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
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
void composite_layer(Image& dst,const Layer& layer){
    if(!layer.visible||layer.opacity==0)return;
    // Clip once, rather than checking every source pixel against the canvas.
    // This skips invisible off-canvas pixels and removes per-pixel bounds checks.
    const int64_t left=layer.x, top=layer.y;
    const int64_t right=left+layer.image.width(), bottom=top+layer.image.height();
    const int64_t x0=std::max<int64_t>(0,left), y0=std::max<int64_t>(0,top);
    const int64_t x1=std::min<int64_t>(dst.width(),right), y1=std::min<int64_t>(dst.height(),bottom);
    if(x0>=x1||y0>=y1)return;
    const uint32_t sx0=static_cast<uint32_t>(x0-left), sy0=static_cast<uint32_t>(y0-top);
    const uint32_t copyWidth=static_cast<uint32_t>(x1-x0), copyHeight=static_cast<uint32_t>(y1-y0);
    const auto& source=layer.image.pixels();
    auto& destination=dst.mutable_pixels();
    const size_t sourceWidth=layer.image.width(), destinationWidth=dst.width();
    const float opacity=layer.opacity/255.0f;
    for(uint32_t row=0;row<copyHeight;++row){
        const size_t sourceStart=static_cast<size_t>(sy0+row)*sourceWidth+sx0;
        const size_t destinationStart=static_cast<size_t>(static_cast<uint32_t>(y0)+row)*destinationWidth+static_cast<uint32_t>(x0);
        for(uint32_t col=0;col<copyWidth;++col){
            const Pixel& s=source[sourceStart+col];Pixel& d=destination[destinationStart+col];
            const float sa=(s.a/255.0f)*opacity;
            if(sa<=0.0f)continue;
            const float da=d.a/255.0f, oa=sa+da*(1.0f-sa);
            if(oa<=0.0f){d={0,0,0,0};continue;}
            auto ch=[&](uint8_t sc,uint8_t dc){
                const float sf=sc/255.0f,df=dc/255.0f,b=blend_channel(sf,df,layer.blend);
                return clamp(((1.0f-sa)*df*da+sa*((1.0f-da)*sf+da*b))/oa*255.0f);
            };
            d.r=ch(s.r,d.r);d.g=ch(s.g,d.g);d.b=ch(s.b,d.b);d.a=clamp(oa*255.0f);
        }
    }
}
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
void Document::rotate_canvas(bool clockwise){
    std::vector<Layer> rotated=layers_;
    const int64_t old_width=width_,old_height=height_;
    for(auto& layer:rotated){
        const int64_t nx=clockwise?old_height-(static_cast<int64_t>(layer.y)+layer.image.height()):layer.y;
        const int64_t ny=clockwise?layer.x:old_width-(static_cast<int64_t>(layer.x)+layer.image.width());
        if(nx<std::numeric_limits<int32_t>::min()||nx>std::numeric_limits<int32_t>::max()||
           ny<std::numeric_limits<int32_t>::min()||ny>std::numeric_limits<int32_t>::max())
            throw std::overflow_error("rotated layer offset exceeds supported range");
        layer.image=clockwise?rotate_90_clockwise(layer.image):rotate_90_counterclockwise(layer.image);
        layer.x=static_cast<int32_t>(nx);layer.y=static_cast<int32_t>(ny);
    }
    layers_=std::move(rotated);
    std::swap(width_,height_);
}
void Document::save(const std::string& path)const{if(layers_.size()>kMaxLayers)throw std::length_error("too many layers");const auto destination=std::filesystem::u8path(path);auto tmp=destination;tmp += ".tmp";try{std::ofstream o(tmp,std::ios::binary|std::ios::trunc);if(!o)throw std::runtime_error("cannot create project file");write_bytes(o,kMagic,sizeof(kMagic));write_le<uint32_t>(o,width_);write_le<uint32_t>(o,height_);write_string(o,name_);write_le<uint32_t>(o,static_cast<uint32_t>(layers_.size()));for(const auto& l:layers_){write_string(o,l.name);write_le<int32_t>(o,l.x);write_le<int32_t>(o,l.y);write_le<uint8_t>(o,l.opacity);write_le<uint8_t>(o,l.visible?1:0);write_le<uint8_t>(o,static_cast<uint8_t>(l.blend));write_le<uint8_t>(o,0);write_le<uint32_t>(o,l.image.width());write_le<uint32_t>(o,l.image.height());for(const auto& p:l.image.pixels()){write_le<uint8_t>(o,p.r);write_le<uint8_t>(o,p.g);write_le<uint8_t>(o,p.b);write_le<uint8_t>(o,p.a);}}o.flush();if(!o)throw std::runtime_error("project flush failed");o.close();if(!replace_file(tmp,destination))throw std::runtime_error("could not finalize project file");}catch(...){std::error_code ec;std::filesystem::remove(tmp,ec);throw;}}
Document Document::load(const std::string& path){std::ifstream i(std::filesystem::u8path(path),std::ios::binary);if(!i)throw std::runtime_error("cannot open project file");char magic[8];read_bytes(i,magic,sizeof(magic));if(std::memcmp(magic,kMagic,sizeof(magic))!=0)throw std::runtime_error("not a PTDOC v1 project");uint32_t w=read_le<uint32_t>(i),h=read_le<uint32_t>(i);Document d(w,h,read_string(i));uint32_t count=read_le<uint32_t>(i);if(count>kMaxLayers)throw std::runtime_error("project has too many layers");for(uint32_t n=0;n<count;++n){std::string name=read_string(i);int32_t x=read_le<int32_t>(i),y=read_le<int32_t>(i);uint8_t opacity=read_le<uint8_t>(i),visible=read_le<uint8_t>(i),blend=read_le<uint8_t>(i);(void)read_le<uint8_t>(i);uint32_t lw=read_le<uint32_t>(i),lh=read_le<uint32_t>(i);if(!lw||!lh||static_cast<uint64_t>(lw)*lh>100000000ULL||blend>static_cast<uint8_t>(BlendMode::Subtract)||visible>1)throw std::runtime_error("invalid layer metadata");Image image(lw,lh);for(auto& p:image.mutable_pixels()){p.r=read_le<uint8_t>(i);p.g=read_le<uint8_t>(i);p.b=read_le<uint8_t>(i);p.a=read_le<uint8_t>(i);}Layer layer(std::move(name),std::move(image));layer.x=x;layer.y=y;layer.opacity=opacity;layer.visible=visible!=0;layer.blend=static_cast<BlendMode>(blend);d.add_layer(std::move(layer));}char extra;if(i.read(&extra,1))throw std::runtime_error("unexpected trailing data in project");return d;}
Document::State Document::snapshot(std::string label)const{return {std::move(label),width_,height_,name_,layers_};}
uint64_t Document::estimate_state_bytes(const State& state) noexcept{uint64_t total=sizeof(State)+state.label.size()+state.name.size();for(const auto& layer:state.layers){const uint64_t pixels=static_cast<uint64_t>(layer.image.width())*layer.image.height();const uint64_t bytes=pixels*sizeof(Pixel);if(bytes>std::numeric_limits<uint64_t>::max()-total)return std::numeric_limits<uint64_t>::max();total+=bytes+sizeof(Layer)+layer.name.size();}return total;}
void Document::restore(const State&s){width_=s.width;height_=s.height;name_=s.name;layers_=s.layers;}
void Document::checkpoint(std::string label){
    constexpr uint64_t maxHistoryBytes=128ULL*1024ULL*1024ULL;
    if(history_cursor_<history_.size())history_.erase(history_.begin()+static_cast<std::ptrdiff_t>(history_cursor_),history_.end());
    uint64_t incomingBytes=sizeof(State)+label.size()+name_.size();
    for(const auto& layer:layers_){const uint64_t bytes=static_cast<uint64_t>(layer.image.width())*layer.image.height()*sizeof(Pixel);if(bytes>std::numeric_limits<uint64_t>::max()-incomingBytes){incomingBytes=std::numeric_limits<uint64_t>::max();break;}incomingBytes+=bytes+sizeof(Layer)+layer.name.size();}
    if(incomingBytes>maxHistoryBytes){history_.clear();history_cursor_=0;return;}
    uint64_t retainedBytes=0;for(const auto& state:history_)retainedBytes+=estimate_state_bytes(state);
    while(!history_.empty()&&retainedBytes>maxHistoryBytes-incomingBytes){retainedBytes-=std::min(retainedBytes,estimate_state_bytes(history_.front()));history_.erase(history_.begin());if(history_cursor_>0)--history_cursor_;}
    history_.push_back({std::move(label),width_,height_,name_,layers_});
    if(history_.size()>kMaxHistory)history_.erase(history_.begin());
    history_cursor_=history_.size();
}
bool Document::undo(){if(history_cursor_<=1)return false;--history_cursor_;restore(history_[history_cursor_-1]);return true;}
bool Document::redo(){if(history_cursor_>=history_.size())return false;restore(history_[history_cursor_]);++history_cursor_;return true;}
bool Document::can_undo()const noexcept{return history_cursor_>1;}bool Document::can_redo()const noexcept{return history_cursor_<history_.size();}
const std::string& Document::undo_label()const noexcept{static const std::string empty;return can_undo()?history_[history_cursor_-1].label:empty;}
const std::string& Document::redo_label()const noexcept{static const std::string empty;return can_redo()?history_[history_cursor_].label:empty;}
}
