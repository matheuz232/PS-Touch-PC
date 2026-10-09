#include "pstouch/image.hpp"
#include <algorithm>
#include <utility>
#include <cmath>
#include <limits>
#include <array>

namespace pstouch {
namespace {
uint8_t clamp_byte(float v) {
    if (!std::isfinite(v)) return 0;
    return static_cast<uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}
}
Image::Image(uint32_t width, uint32_t height, Pixel fill) : width_(width), height_(height) {
    if (!width || !height) throw std::invalid_argument("image dimensions must be non-zero");
    const uint64_t count = static_cast<uint64_t>(width) * height;
    if (count > 100000000ULL || count > std::numeric_limits<size_t>::max() / sizeof(Pixel))
        throw std::length_error("image dimensions exceed safety limit");
    pixels_.assign(static_cast<size_t>(count), fill);
}
Pixel& Image::at(uint32_t x, uint32_t y) {
    if (x >= width_ || y >= height_) throw std::out_of_range("pixel coordinate out of range");
    return pixels_[static_cast<size_t>(y) * width_ + x];
}
const Pixel& Image::at(uint32_t x, uint32_t y) const {
    if (x >= width_ || y >= height_) throw std::out_of_range("pixel coordinate out of range");
    return pixels_[static_cast<size_t>(y) * width_ + x];
}
void premultiply_alpha(Image& image) {
    for (auto& p : image.mutable_pixels()) {
        const float a = p.a / 255.0f;
        p.r = clamp_byte(p.r * a); p.g = clamp_byte(p.g * a); p.b = clamp_byte(p.b * a);
    }
}
void unpremultiply_alpha(Image& image) {
    for (auto& p : image.mutable_pixels()) {
        if (p.a == 0) { p.r = p.g = p.b = 0; continue; }
        const float k = 255.0f / p.a;
        p.r = clamp_byte(p.r * k); p.g = clamp_byte(p.g * k); p.b = clamp_byte(p.b * k);
    }
}
void source_over(Image& dst, const Image& src, int32_t ox, int32_t oy) {
    // Clip the overlap once, then walk contiguous rows to avoid per-pixel
    // coordinate arithmetic and repeated bounds checks on large canvases.
    const int64_t left=ox, top=oy;
    const int64_t sx0=std::max<int64_t>(0,-left), sy0=std::max<int64_t>(0,-top);
    const int64_t sx1=std::min<int64_t>(src.width(),static_cast<int64_t>(dst.width())-left);
    const int64_t sy1=std::min<int64_t>(src.height(),static_cast<int64_t>(dst.height())-top);
    if(sx0>=sx1||sy0>=sy1)return;
    const size_t sourceWidth=src.width(), destinationWidth=dst.width();
    const size_t copyWidth=static_cast<size_t>(sx1-sx0), copyHeight=static_cast<size_t>(sy1-sy0);
    const size_t sourceX=static_cast<size_t>(sx0), sourceY=static_cast<size_t>(sy0);
    const size_t destinationX=static_cast<size_t>(left+sx0), destinationY=static_cast<size_t>(top+sy0);
    const auto& source=src.pixels();
    auto& destination=dst.mutable_pixels();
    for(size_t row=0;row<copyHeight;++row){
        const size_t sourceStart=(sourceY+row)*sourceWidth+sourceX;
        const size_t destinationStart=(destinationY+row)*destinationWidth+destinationX;
        for(size_t col=0;col<copyWidth;++col){
            const Pixel& sp=source[sourceStart+col];
            Pixel& dp=destination[destinationStart+col];
            const float sa=sp.a/255.0f, da=dp.a/255.0f, oa=sa+da*(1.0f-sa);
            if(oa<=0.0f){dp={0,0,0,0};continue;}
            const auto blend=[&](uint8_t sc,uint8_t dc){return clamp_byte((sc*sa+dc*da*(1.0f-sa))/oa);};
            dp.r=blend(sp.r,dp.r);dp.g=blend(sp.g,dp.g);dp.b=blend(sp.b,dp.b);dp.a=clamp_byte(oa*255.0f);
        }
    }
}
Image resample_bilinear(const Image& src, uint32_t width, uint32_t height) {
    Image out(width, height);
    for (uint32_t y = 0; y < height; ++y) {
        const float fy = std::clamp((static_cast<float>(y) + 0.5f) * static_cast<float>(src.height()) / static_cast<float>(height) - 0.5f, 0.0f, static_cast<float>(src.height()-1));
        const uint32_t y0 = static_cast<uint32_t>(fy), y1 = std::min(y0 + 1, src.height()-1); const float ty = fy-static_cast<float>(y0);
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = std::clamp((static_cast<float>(x) + 0.5f) * static_cast<float>(src.width()) / static_cast<float>(width) - 0.5f, 0.0f, static_cast<float>(src.width()-1));
            const uint32_t x0 = static_cast<uint32_t>(fx), x1 = std::min(x0 + 1, src.width()-1); const float tx = fx-static_cast<float>(x0);
            const Pixel p00=src.at(x0,y0), p10=src.at(x1,y0), p01=src.at(x0,y1), p11=src.at(x1,y1);
            auto channel = [&](uint8_t Pixel::*m) { const float a=p00.*m+(p10.*m-p00.*m)*tx, b=p01.*m+(p11.*m-p01.*m)*tx; return clamp_byte(a+(b-a)*ty); };
            out.at(x,y) = {channel(&Pixel::r),channel(&Pixel::g),channel(&Pixel::b),channel(&Pixel::a)};
        }
    }
    return out;
}
void brightness_contrast(Image& image, float brightness, float contrast) {
    if (!std::isfinite(brightness) || !std::isfinite(contrast)) throw std::invalid_argument("brightness/contrast must be finite");
    brightness = std::clamp(brightness, -1.0f, 1.0f);
    contrast = std::clamp(contrast, -1.0f, 1.0f);
    // Mirrors the formula in assets/resource/application/filters/contrastbrightness.fs.
    // Input/output channels are normalized RGBA values, with RGB constrained by alpha.
    const float wContrast = contrast * contrast * (contrast < 0.0f ? -1.0f : 1.0f);
    const float wBrightness = brightness * brightness * (brightness < 0.0f ? -1.0f : 1.0f);
    for (auto& p : image.mutable_pixels()) {
        const float alpha = p.a / 255.0f;
        auto apply = [&](uint8_t channel) {
            const float c = channel / 255.0f;
            float adjustedContrast;
            if (wContrast < 0.0f) adjustedContrast = std::max((c - 0.5f) * (1.0f + wContrast) + 0.5f, 0.0f);
            else adjustedContrast = std::min(c + (c - 0.5f) * wContrast, 1.0f);
            float adjustedBrightness;
            if (wBrightness < 0.0f) adjustedBrightness = adjustedContrast * (1.0f + wBrightness);
            else adjustedBrightness = adjustedContrast + wBrightness;
            return clamp_byte(std::min(adjustedBrightness, alpha) * 255.0f);
        };
        p.r=apply(p.r); p.g=apply(p.g); p.b=apply(p.b);
    }
}

Image crop(const Image& image,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    if(width==0||height==0||x>image.width()||y>image.height()||width>image.width()-x||height>image.height()-y)throw std::invalid_argument("crop rectangle outside image");
    Image out(width,height);for(uint32_t yy=0;yy<height;++yy)for(uint32_t xx=0;xx<width;++xx)out.at(xx,yy)=image.at(x+xx,y+yy);return out;
}
void flip_horizontal(Image& image){for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width()/2U;++x)std::swap(image.at(x,y),image.at(image.width()-1U-x,y));}
void flip_vertical(Image& image){for(uint32_t y=0;y<image.height()/2U;++y)for(uint32_t x=0;x<image.width();++x)std::swap(image.at(x,y),image.at(x,image.height()-1U-y));}
Image rotate_90_clockwise(const Image& image){Image out(image.height(),image.width());for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width();++x)out.at(image.height()-1U-y,x)=image.at(x,y);return out;}
Image rotate_90_counterclockwise(const Image& image){Image out(image.height(),image.width());for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width();++x)out.at(y,image.width()-1U-x)=image.at(x,y);return out;}
void grayscale(Image& image){for(auto& p:image.mutable_pixels()){const float y=0.2126f*p.r+0.7152f*p.g+0.0722f*p.b;const auto v=clamp_byte(y);p.r=p.g=p.b=v;}}
void sepia(Image& image){for(auto& p:image.mutable_pixels()){const float r=p.r,g=p.g,b=p.b;p.r=clamp_byte(0.393f*r+0.769f*g+0.189f*b);p.g=clamp_byte(0.349f*r+0.686f*g+0.168f*b);p.b=clamp_byte(0.272f*r+0.534f*g+0.131f*b);}}
void adjust_saturation(Image& image,float amount){if(!std::isfinite(amount))throw std::invalid_argument("saturation must be finite");amount=std::clamp(amount,-1.0f,1.0f);const float factor=1.0f+amount;for(auto& p:image.mutable_pixels()){const float gray=0.2126f*p.r+0.7152f*p.g+0.0722f*p.b;p.r=clamp_byte(gray+(p.r-gray)*factor);p.g=clamp_byte(gray+(p.g-gray)*factor);p.b=clamp_byte(gray+(p.b-gray)*factor);}}
void invert_colors(Image& image){for(auto& p:image.mutable_pixels()){p.r=static_cast<uint8_t>(255U-p.r);p.g=static_cast<uint8_t>(255U-p.g);p.b=static_cast<uint8_t>(255U-p.b);}}
void posterize(Image& image,uint16_t levels){if(levels<2||levels>256)throw std::invalid_argument("posterize levels must be between 2 and 256");if(levels==256)return;const float scale=255.0f/static_cast<float>(levels-1U);for(auto& p:image.mutable_pixels()){auto quantize=[&](uint8_t channel){const auto level=std::lround(static_cast<float>(channel)/255.0f*static_cast<float>(levels-1U));return static_cast<uint8_t>(std::clamp(std::lround(static_cast<float>(level)*scale),0L,255L));};p.r=quantize(p.r);p.g=quantize(p.g);p.b=quantize(p.b);}}
void gaussian_blur(Image& image,float sigma){
    if(!std::isfinite(sigma)||sigma<=0.0f||sigma>64.0f)throw std::invalid_argument("blur sigma must be in (0, 64]");
    const int radius=std::max(1,static_cast<int>(std::ceil(3.0f*sigma)));
    std::vector<float> kernel(static_cast<size_t>(radius*2+1));float sum=0.0f;
    for(int i=-radius;i<=radius;++i){const float v=std::exp(-static_cast<float>(i*i)/(2.0f*sigma*sigma));kernel[static_cast<size_t>(i+radius)]=v;sum+=v;}
    for(auto& v:kernel)v/=sum;
    const uint32_t w=image.width(),h=image.height();std::vector<Pixel> temp(image.pixels().size());
    const auto& src=image.pixels();
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){float ch[4]={0,0,0,0};for(int k=-radius;k<=radius;++k){const auto xx=static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(x)+k,0,static_cast<int64_t>(w)-1));const auto& p=src[static_cast<size_t>(y)*w+xx];const float weight=kernel[static_cast<size_t>(k+radius)];ch[0]+=p.r*weight;ch[1]+=p.g*weight;ch[2]+=p.b*weight;ch[3]+=p.a*weight;}temp[static_cast<size_t>(y)*w+x]={clamp_byte(ch[0]),clamp_byte(ch[1]),clamp_byte(ch[2]),clamp_byte(ch[3])};}
    auto& dst=image.mutable_pixels();
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){float ch[4]={0,0,0,0};for(int k=-radius;k<=radius;++k){const auto yy=static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(y)+k,0,static_cast<int64_t>(h)-1));const auto& p=temp[static_cast<size_t>(yy)*w+x];const float weight=kernel[static_cast<size_t>(k+radius)];ch[0]+=p.r*weight;ch[1]+=p.g*weight;ch[2]+=p.b*weight;ch[3]+=p.a*weight;}dst[static_cast<size_t>(y)*w+x]={clamp_byte(ch[0]),clamp_byte(ch[1]),clamp_byte(ch[2]),clamp_byte(ch[3])};}
}
void sharpen(Image& image,float amount){
    if(!std::isfinite(amount)||amount<0.0f||amount>5.0f)throw std::invalid_argument("sharpen amount must be in [0, 5]");
    if(amount==0.0f)return;
    // Fuse unsharp masking with the vertical blur pass. Only one full-frame
    // temporary is needed, instead of a copied Image plus gaussian_blur's buffer.
    constexpr float sigma=1.0f;
    constexpr int radius=3;
    std::array<float,7> kernel{};
    float sum=0.0f;
    for(int i=-radius;i<=radius;++i){
        const float value=std::exp(-static_cast<float>(i*i)/(2.0f*sigma*sigma));
        kernel[static_cast<size_t>(i+radius)]=value;
        sum+=value;
    }
    for(auto& value:kernel)value/=sum;
    const uint32_t w=image.width(),h=image.height();
    const auto& src=image.pixels();
    std::vector<Pixel> horizontal(src.size());
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
        float channels[4]={0,0,0,0};
        for(int k=-radius;k<=radius;++k){
            const auto xx=static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(x)+k,0,static_cast<int64_t>(w)-1));
            const auto& p=src[static_cast<size_t>(y)*w+xx];
            const float weight=kernel[static_cast<size_t>(k+radius)];
            channels[0]+=p.r*weight;channels[1]+=p.g*weight;
            channels[2]+=p.b*weight;channels[3]+=p.a*weight;
        }
        horizontal[static_cast<size_t>(y)*w+x]={clamp_byte(channels[0]),clamp_byte(channels[1]),clamp_byte(channels[2]),clamp_byte(channels[3])};
    }
    auto& dst=image.mutable_pixels();
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
        float channels[3]={0,0,0};
        for(int k=-radius;k<=radius;++k){
            const auto yy=static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(y)+k,0,static_cast<int64_t>(h)-1));
            const auto& p=horizontal[static_cast<size_t>(yy)*w+x];
            const float weight=kernel[static_cast<size_t>(k+radius)];
            channels[0]+=p.r*weight;channels[1]+=p.g*weight;channels[2]+=p.b*weight;
        }
        auto& out=dst[static_cast<size_t>(y)*w+x];
        const auto channel=[&](uint8_t original,float blurred){return clamp_byte(static_cast<float>(original)+(static_cast<float>(original)-clamp_byte(blurred))*amount);};
        out.r=channel(out.r,channels[0]);out.g=channel(out.g,channels[1]);out.b=channel(out.b,channels[2]);
    }
}
void edge_detect(Image& image){
    const uint32_t w=image.width(),h=image.height();
    const auto& src=image.pixels();
    auto& dst=image.mutable_pixels();
    // A three-row luminance ring buffer replaces a full-frame source copy.
    // This materially reduces peak memory on large images and low-RAM PCs.
    std::vector<float> previous(w),current(w),next(w);
    const auto load_row=[&](uint32_t y,std::vector<float>& row){
        const size_t start=static_cast<size_t>(y)*w;
        for(uint32_t x=0;x<w;++x){
            const auto& p=src[start+x];
            row[x]=0.2126f*p.r+0.7152f*p.g+0.0722f*p.b;
        }
    };
    load_row(0,previous);
    load_row(0,current);
    load_row(std::min<uint32_t>(1,h-1),next);
    for(uint32_t y=0;y<h;++y){
        for(uint32_t x=0;x<w;++x){
            const uint32_t left=x==0?0:x-1,right=std::min<uint32_t>(x+1,w-1);
            const float gx=-previous[left]+previous[right]-2.0f*current[left]+2.0f*current[right]-next[left]+next[right];
            const float gy=-previous[left]-2.0f*previous[x]-previous[right]+next[left]+2.0f*next[x]+next[right];
            const auto value=clamp_byte(std::sqrt(gx*gx+gy*gy));
            auto& p=dst[static_cast<size_t>(y)*w+x];
            p.r=p.g=p.b=value;
        }
        if(y+1<h){
            previous.swap(current);
            current.swap(next);
            load_row(std::min<uint32_t>(y+2,h-1),next);
        }
    }
}
void threshold(Image& image,uint8_t cutoff){for(auto& p:image.mutable_pixels()){const auto y=clamp_byte(0.2126f*p.r+0.7152f*p.g+0.0722f*p.b);const uint8_t v=y>=cutoff?255:0;p.r=p.g=p.b=v;}}
void adjust_gamma(Image& image,float gamma){
    if(!std::isfinite(gamma)||gamma<=0.0f||gamma>10.0f)throw std::invalid_argument("gamma must be in (0, 10]");
    std::array<uint8_t,256> lut{};for(size_t i=0;i<lut.size();++i)lut[i]=clamp_byte(255.0f*std::pow(static_cast<float>(i)/255.0f,1.0f/gamma));
    for(auto& p:image.mutable_pixels()){p.r=lut[p.r];p.g=lut[p.g];p.b=lut[p.b];}
}
void adjust_temperature(Image& image,float amount){
    if(!std::isfinite(amount)||amount< -1.0f||amount>1.0f)throw std::invalid_argument("temperature must be in [-1, 1]");
    const float shift=amount*48.0f;for(auto& p:image.mutable_pixels()){p.r=clamp_byte(p.r+shift);p.b=clamp_byte(p.b-shift);}
}
void vignette(Image& image,float amount){
    if(!std::isfinite(amount)||amount<0.0f||amount>1.0f)throw std::invalid_argument("vignette amount must be in [0, 1]");
    if(amount==0.0f)return;const float cx=(static_cast<float>(image.width())-1.0f)*0.5f,cy=(static_cast<float>(image.height())-1.0f)*0.5f;const float maxDist=std::sqrt(cx*cx+cy*cy);if(maxDist<=0.0f)return;
    for(uint32_t y=0;y<image.height();++y)for(uint32_t x=0;x<image.width();++x){const float dx=(static_cast<float>(x)-cx)/maxDist,dy=(static_cast<float>(y)-cy)/maxDist;const float factor=1.0f-amount*std::clamp((dx*dx+dy*dy)*1.35f,0.0f,1.0f);auto& p=image.mutable_pixels()[static_cast<size_t>(y)*image.width()+x];p.r=clamp_byte(p.r*factor);p.g=clamp_byte(p.g*factor);p.b=clamp_byte(p.b*factor);}
}
void pixelate(Image& image,uint32_t blockSize){
    if(blockSize==0||blockSize>4096)throw std::invalid_argument("pixel block size must be in [1, 4096]");if(blockSize==1)return;
    const uint32_t w=image.width(),h=image.height();auto& px=image.mutable_pixels();
    for(uint32_t by=0;by<h;by+=blockSize)for(uint32_t bx=0;bx<w;bx+=blockSize){const uint32_t ex=std::min(w,bx+blockSize),ey=std::min(h,by+blockSize);uint64_t r=0,g=0,b=0,a=0,count=0;for(uint32_t y=by;y<ey;++y)for(uint32_t x=bx;x<ex;++x){const auto& p=px[static_cast<size_t>(y)*w+x];r+=p.r;g+=p.g;b+=p.b;a+=p.a;++count;}const Pixel avg{static_cast<uint8_t>(r/count),static_cast<uint8_t>(g/count),static_cast<uint8_t>(b/count),static_cast<uint8_t>(a/count)};for(uint32_t y=by;y<ey;++y)for(uint32_t x=bx;x<ex;++x)px[static_cast<size_t>(y)*w+x]=avg;}
}


void adjust_exposure(Image& image,float stops){
    if(!std::isfinite(stops)||stops< -8.0f||stops>8.0f)throw std::invalid_argument("exposure stops must be in [-8, 8]");
    const float factor=std::exp2(stops);
    for(auto& p:image.mutable_pixels()){p.r=clamp_byte(p.r*factor);p.g=clamp_byte(p.g*factor);p.b=clamp_byte(p.b*factor);}
}
void adjust_hue(Image& image,float degrees){
    if(!std::isfinite(degrees)||degrees< -180.0f||degrees>180.0f)throw std::invalid_argument("hue rotation must be in [-180, 180]");
    if(degrees==0.0f)return;
    const float shift=degrees/60.0f;
    for(auto& p:image.mutable_pixels()){
        const float r=p.r/255.0f,g=p.g/255.0f,b=p.b/255.0f;
        const float hi=std::max({r,g,b}),lo=std::min({r,g,b}),delta=hi-lo;
        float h=0.0f,s=hi==0.0f?0.0f:delta/hi;
        if(delta>0.0f){if(hi==r)h=std::fmod((g-b)/delta,6.0f);else if(hi==g)h=(b-r)/delta+2.0f;else h=(r-g)/delta+4.0f;h+=shift;h=std::fmod(h,6.0f);if(h<0.0f)h+=6.0f;}
        const float chroma=hi*s,x=chroma*(1.0f-std::fabs(std::fmod(h,2.0f)-1.0f)),m=hi-chroma;
        float nr=0.0f,ng=0.0f,nb=0.0f;
        if(h<1.0f){nr=chroma;ng=x;}else if(h<2.0f){nr=x;ng=chroma;}else if(h<3.0f){ng=chroma;nb=x;}else if(h<4.0f){ng=x;nb=chroma;}else if(h<5.0f){nr=x;nb=chroma;}else{nr=chroma;nb=x;}
        p.r=clamp_byte((nr+m)*255.0f);p.g=clamp_byte((ng+m)*255.0f);p.b=clamp_byte((nb+m)*255.0f);
    }
}
void adjust_levels(Image& image,uint8_t blackPoint,uint8_t whitePoint,float gamma){
    if(blackPoint>=whitePoint)throw std::invalid_argument("levels black point must be below white point");
    if(!std::isfinite(gamma)||gamma<=0.0f||gamma>10.0f)throw std::invalid_argument("levels gamma must be in (0, 10]");
    std::array<uint8_t,256> lut{};
    const float range=static_cast<float>(whitePoint-blackPoint);
    for(size_t i=0;i<lut.size();++i){const float normalized=std::clamp((static_cast<float>(i)-blackPoint)/range,0.0f,1.0f);lut[i]=clamp_byte(std::pow(normalized,1.0f/gamma)*255.0f);}
    for(auto& p:image.mutable_pixels()){p.r=lut[p.r];p.g=lut[p.g];p.b=lut[p.b];}
}
void auto_contrast(Image& image){
    if(image.pixels().empty())return;
    uint8_t minR=255,minG=255,minB=255,maxR=0,maxG=0,maxB=0;
    bool foundVisible=false;
    for(const auto& p:image.pixels()){
        if(p.a==0)continue;
        foundVisible=true;
        minR=std::min(minR,p.r);minG=std::min(minG,p.g);minB=std::min(minB,p.b);
        maxR=std::max(maxR,p.r);maxG=std::max(maxG,p.g);maxB=std::max(maxB,p.b);
    }
    if(!foundVisible)return;
    const bool varyR=maxR>minR,varyG=maxG>minG,varyB=maxB>minB;
    for(auto& p:image.mutable_pixels()){
        if(p.a==0)continue;
        if(varyR)p.r=static_cast<uint8_t>((static_cast<uint32_t>(p.r-minR)*255U)/(maxR-minR));
        if(varyG)p.g=static_cast<uint8_t>((static_cast<uint32_t>(p.g-minG)*255U)/(maxG-minG));
        if(varyB)p.b=static_cast<uint8_t>((static_cast<uint32_t>(p.b-minB)*255U)/(maxB-minB));
    }
}


void adjust_shadows_highlights(Image& image,float shadows,float highlights){
    if(!std::isfinite(shadows)||!std::isfinite(highlights)||shadows< -1.0f||shadows>1.0f||highlights< -1.0f||highlights>1.0f)throw std::invalid_argument("shadows and highlights must be in [-1, 1]");
    if(shadows==0.0f&&highlights==0.0f)return;
    auto tone=[&](uint8_t value){
        const float v=value/255.0f;
        const float shadowWeight=(1.0f-v)*(1.0f-v);
        const float highlightWeight=v*v;
        const float adjusted=v+shadows*shadowWeight*0.65f+highlights*highlightWeight*0.65f;
        return clamp_byte(adjusted*255.0f);
    };
    for(auto& p:image.mutable_pixels()){p.r=tone(p.r);p.g=tone(p.g);p.b=tone(p.b);}
}
void color_balance(Image& image,float red,float green,float blue){
    if(!std::isfinite(red)||!std::isfinite(green)||!std::isfinite(blue)||red< -1.0f||red>1.0f||green< -1.0f||green>1.0f||blue< -1.0f||blue>1.0f)throw std::invalid_argument("color balance channels must be in [-1, 1]");
    const float rGain=std::exp2(red),gGain=std::exp2(green),bGain=std::exp2(blue);
    for(auto& p:image.mutable_pixels()){p.r=clamp_byte(p.r*rGain);p.g=clamp_byte(p.g*gGain);p.b=clamp_byte(p.b*bGain);}
}


void adjust_vibrance(Image& image,float amount){
    if(!std::isfinite(amount)||amount< -1.0f||amount>1.0f)
        throw std::invalid_argument("vibrance amount must be in [-1, 1]");
    if(amount==0.0f)return;
    for(auto& p:image.mutable_pixels()){
        const float r=static_cast<float>(p.r),g=static_cast<float>(p.g),b=static_cast<float>(p.b);
        const float hi=std::max({r,g,b}),lo=std::min({r,g,b});
        const float saturation=(hi-lo)/255.0f;
        const float luma=0.2126f*r+0.7152f*g+0.0722f*b;
        const float scale=1.0f+amount*(1.0f-saturation);
        p.r=clamp_byte(luma+(r-luma)*scale);
        p.g=clamp_byte(luma+(g-luma)*scale);
        p.b=clamp_byte(luma+(b-luma)*scale);
    }
}


void equalize_luminance(Image& image){
    if(image.pixels().empty())return;
    std::array<uint64_t,256> histogram{};
    uint64_t total=0;
    for(const auto& p:image.pixels()){
        // Fully transparent pixels do not contribute to visible image statistics.
        if(p.a==0)continue;
        const auto y=clamp_byte(0.2126f*p.r+0.7152f*p.g+0.0722f*p.b);
        ++histogram[y];
        ++total;
    }
    uint64_t first=0,cumulative=0;
    bool found=false;
    for(size_t i=0;i<histogram.size();++i){
        cumulative+=histogram[i];
        if(!found&&histogram[i]!=0){first=cumulative;found=true;}
    }
    if(!found||total<=first)return;
    std::array<uint8_t,256> lut{};
    cumulative=0;
    for(size_t i=0;i<histogram.size();++i){
        cumulative+=histogram[i];
        const uint64_t adjusted=cumulative>first?cumulative-first:0;
        lut[i]=clamp_byte(static_cast<float>(adjusted)*255.0f/static_cast<float>(total-first));
    }
    for(auto& p:image.mutable_pixels()){
        if(p.a==0)continue;
        const float oldY=0.2126f*p.r+0.7152f*p.g+0.0722f*p.b;
        if(oldY<=0.0f)continue;
        const float scale=lut[clamp_byte(oldY)]/oldY;
        p.r=clamp_byte(p.r*scale);p.g=clamp_byte(p.g*scale);p.b=clamp_byte(p.b*scale);
    }
}

}
