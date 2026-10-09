#include "pstouch/image.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cstdint>
using namespace pstouch;
int main() {
    Image transparent(1,1,{200,100,50,0});
    premultiply_alpha(transparent); assert(transparent.at(0,0).r==0 && transparent.at(0,0).g==0);
    unpremultiply_alpha(transparent); assert(transparent.at(0,0).r==0 && transparent.at(0,0).a==0);
    Image half(1,1,{100,50,25,128}); premultiply_alpha(half);
    assert(half.at(0,0).r==50 && half.at(0,0).g==25 && half.at(0,0).b==13);
    unpremultiply_alpha(half); assert(half.at(0,0).r==100 && half.at(0,0).g==50);
    Image dst(2,2,{0,0,255,255}); Image src(1,1,{255,0,0,128}); source_over(dst,src,0,0);
    assert(dst.at(0,0).r==128 && dst.at(0,0).b==127 && dst.at(0,0).a==255);
    Image translucentDst(1,1,{0,100,200,128});Image translucentSrc(1,1,{200,100,0,128});source_over(translucentDst,translucentSrc,0,0);
    assert(translucentDst.at(0,0).r==134&&translucentDst.at(0,0).g==100&&translucentDst.at(0,0).b==66&&translucentDst.at(0,0).a==192);
    Image transparentDst(1,1,{91,82,73,0});Image transparentSrc(1,1,{20,40,60,0});source_over(transparentDst,transparentSrc,0,0);
    assert(transparentDst.at(0,0).r==0&&transparentDst.at(0,0).g==0&&transparentDst.at(0,0).b==0&&transparentDst.at(0,0).a==0);
    source_over(dst,src,-1,-1); assert(dst.at(1,1).b==255);
    Image corner(2,2,{0,255,0,255});source_over(dst,corner,1,1);
    assert(dst.at(1,1).g==255&&dst.at(1,1).r==0);
    const auto beforeOffscreen=dst.pixels();source_over(dst,corner,INT32_MAX,INT32_MAX);
    for(size_t i=0;i<beforeOffscreen.size();++i){const auto actual=dst.pixels()[i],expected=beforeOffscreen[i];assert(actual.r==expected.r&&actual.g==expected.g&&actual.b==expected.b&&actual.a==expected.a);}
    Image ramp(2,1,{0,0,0,255}); ramp.at(1,0)={100,100,100,255};
    Image mid=resample_bilinear(ramp,3,1); assert(mid.width()==3 && mid.at(1,0).r==50);
    Image unchanged(1,1,{100,120,140,255}); brightness_contrast(unchanged,0.0f,0.0f);
    assert(unchanged.at(0,0).r==100 && unchanged.at(0,0).g==120 && unchanged.at(0,0).b==140);
    Image bright(1,1,{100,120,140,255}); brightness_contrast(bright,0.1f,0.0f);
    assert(bright.at(0,0).r==103 && bright.at(0,0).g==123 && bright.at(0,0).b==143);
    Image dark(1,1,{100,120,140,255}); brightness_contrast(dark,-0.1f,0.0f);
    assert(dark.at(0,0).r==99 && dark.at(0,0).g==119 && dark.at(0,0).b==139);
    Image black(1,1,{0,0,0,255}); brightness_contrast(black,0.0f,1.0f);
    assert(black.at(0,0).r==0 && black.at(0,0).g==0 && black.at(0,0).b==0);
    Image white(1,1,{255,255,255,255}); brightness_contrast(white,0.0f,-1.0f);
    assert(white.at(0,0).r==128 && white.at(0,0).g==128 && white.at(0,0).b==128);
    Image alphaBound(1,1,{200,200,200,64}); brightness_contrast(alphaBound,1.0f,1.0f);
    assert(alphaBound.at(0,0).r==64 && alphaBound.at(0,0).g==64 && alphaBound.at(0,0).b==64 && alphaBound.at(0,0).a==64);
    bool threw=false; try { Image invalid(0,1); } catch (const std::invalid_argument&) { threw=true; } assert(threw);
    std::cout << "PASS: alpha, compositing, resampling, brightness/contrast, bounds\n";
}
