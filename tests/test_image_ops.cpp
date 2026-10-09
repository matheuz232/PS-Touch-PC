#include "pstouch/image.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){
 pstouch::Image a(3,2);a.at(0,0)={255,0,0,255};a.at(1,0)={0,255,0,128};a.at(2,0)={0,0,255,0};a.at(0,1)={10,20,30,255};a.at(1,1)={40,50,60,255};a.at(2,1)={70,80,90,255};
 auto c=pstouch::crop(a,1,0,2,2);assert(c.width()==2&&c.height()==2&&c.at(0,0).g==255&&c.at(1,1).r==70);bool bad=false;try{(void)pstouch::crop(a,2,1,2,2);}catch(const std::invalid_argument&){bad=true;}assert(bad);
 auto cw=pstouch::rotate_90_clockwise(a);assert(cw.width()==2&&cw.height()==3&&cw.at(0,0).r==10&&cw.at(0,2).r==70);auto ccw=pstouch::rotate_90_counterclockwise(cw);assert(ccw.width()==3&&ccw.height()==2&&ccw.at(0,0).r==255&&ccw.at(2,1).r==70);
 auto f=a;pstouch::flip_horizontal(f);assert(f.at(0,0).b==255&&f.at(2,0).r==255);pstouch::flip_vertical(f);assert(f.at(0,0).r==70&&f.at(2,1).r==255);
 auto gray=a;pstouch::grayscale(gray);for(const auto& p:gray.pixels())assert(p.r==p.g&&p.g==p.b);assert(gray.at(1,0).a==128);
 auto sep=a;pstouch::sepia(sep);assert(sep.at(0,0).r==100&&sep.at(0,0).g==89&&sep.at(0,0).b==69&&sep.at(0,0).a==255);
 auto sat=a;pstouch::adjust_saturation(sat,-1);for(const auto& p:sat.pixels())assert(p.r==p.g&&p.g==p.b);bad=false;try{pstouch::adjust_saturation(sat,NAN);}catch(const std::invalid_argument&){bad=true;}assert(bad);
 pstouch::Image inverted(2,1,{10,100,250,128});inverted.at(1,0)={0,127,255,0};pstouch::invert_colors(inverted);assert(inverted.at(0,0).r==245&&inverted.at(0,0).g==155&&inverted.at(0,0).b==5&&inverted.at(0,0).a==128);assert(inverted.at(1,0).r==255&&inverted.at(1,0).g==128&&inverted.at(1,0).b==0&&inverted.at(1,0).a==0);
 pstouch::Image poster(3,1,{0,63,255,255});poster.at(1,0)={64,127,192,100};pstouch::posterize(poster,2);assert(poster.at(0,0).r==0&&poster.at(0,0).g==0&&poster.at(0,0).b==255);assert(poster.at(1,0).r==0&&poster.at(1,0).g==0&&poster.at(1,0).b==255&&poster.at(1,0).a==100);pstouch::posterize(poster,256);assert(poster.at(1,0).a==100);bad=false;try{pstouch::posterize(poster,1);}catch(const std::invalid_argument&){bad=true;}assert(bad);
 pstouch::Image blur(3,1,{0,0,0,255});blur.at(1,0)={255,255,255,255};pstouch::gaussian_blur(blur,1.0f);assert(blur.at(1,0).r<255&&blur.at(1,0).r>blur.at(0,0).r);
 pstouch::Image sharp(3,1,{100,100,100,255});sharp.at(1,0)={150,150,150,71};pstouch::sharpen(sharp,1.0f);assert(sharp.at(1,0).r>=150&&sharp.at(1,0).a==71&&sharp.at(0,0).a==255);
 auto noSharpen=pstouch::Image(1,1,{40,80,120,37});pstouch::sharpen(noSharpen,0.0f);assert(noSharpen.at(0,0).r==40&&noSharpen.at(0,0).g==80&&noSharpen.at(0,0).b==120&&noSharpen.at(0,0).a==37);
 pstouch::Image edges(3,3,{0,0,0,255});for(uint32_t y=0;y<3;++y)edges.at(2,y)={255,255,255,255};pstouch::edge_detect(edges);assert(edges.at(1,1).r>0&&edges.at(1,1).r==edges.at(1,1).g);
 pstouch::Image singleEdgePixel(1,1,{120,80,40,37});pstouch::edge_detect(singleEdgePixel);assert(singleEdgePixel.at(0,0).r==0&&singleEdgePixel.at(0,0).g==0&&singleEdgePixel.at(0,0).b==0&&singleEdgePixel.at(0,0).a==37);
 pstouch::Image binary(2,1,{30,30,30,255});binary.at(1,0)={200,200,200,64};pstouch::threshold(binary,128);assert(binary.at(0,0).r==0&&binary.at(1,0).r==255&&binary.at(1,0).a==64);
 pstouch::Image gamma(1,1,{64,128,200,80});pstouch::adjust_gamma(gamma,2.0f);assert(gamma.at(0,0).r>64&&gamma.at(0,0).a==80);
 pstouch::Image temp(1,1,{100,100,100,77});pstouch::adjust_temperature(temp,1.0f);assert(temp.at(0,0).r>100&&temp.at(0,0).b<100&&temp.at(0,0).a==77);
 pstouch::Image shade(3,3,{200,200,200,255});pstouch::vignette(shade,1.0f);assert(shade.at(1,1).r>shade.at(0,0).r);
 pstouch::Image blocks(2,1,{0,0,0,255});blocks.at(1,0)={200,100,50,255};pstouch::pixelate(blocks,2);assert(blocks.at(0,0).r==100&&blocks.at(1,0).r==100);
 bad=false;try{pstouch::gaussian_blur(blur,0.0f);}catch(const std::invalid_argument&){bad=true;}assert(bad);bad=false;try{pstouch::pixelate(blocks,0);}catch(const std::invalid_argument&){bad=true;}assert(bad);

 pstouch::Image exposure(1,1,{40,80,120,91});pstouch::adjust_exposure(exposure,1.0f);assert(exposure.at(0,0).r==80&&exposure.at(0,0).g==160&&exposure.at(0,0).b==240&&exposure.at(0,0).a==91);
 pstouch::Image hue(1,1,{255,0,0,73});pstouch::adjust_hue(hue,120.0f);assert(hue.at(0,0).g>240&&hue.at(0,0).r<10&&hue.at(0,0).a==73);
 pstouch::Image levels(3,1,{20,30,40,255});levels.at(1,0)={120,130,140,100};levels.at(2,0)={220,230,240,0};pstouch::adjust_levels(levels,20,220,1.0f);assert(levels.at(0,0).r==0&&levels.at(2,0).r==255&&levels.at(1,0).a==100);
 pstouch::Image autoC(2,1,{20,50,100,64});autoC.at(1,0)={220,150,200,128};pstouch::auto_contrast(autoC);assert(autoC.at(0,0).r==0&&autoC.at(1,0).r==255&&autoC.at(0,0).g==0&&autoC.at(1,0).g==255&&autoC.at(0,0).a==64&&autoC.at(1,0).a==128);
 pstouch::Image autoVisible(2,1,{20,40,60,255});autoVisible.at(1,0)={220,180,140,255};
 pstouch::Image autoHidden(3,1,{20,40,60,255});autoHidden.at(1,0)={220,180,140,255};autoHidden.at(2,0)={255,0,255,0};
 pstouch::auto_contrast(autoVisible);pstouch::auto_contrast(autoHidden);
 assert(autoVisible.at(0,0).r==autoHidden.at(0,0).r&&autoVisible.at(1,0).g==autoHidden.at(1,0).g);
 assert(autoHidden.at(2,0).r==255&&autoHidden.at(2,0).b==255&&autoHidden.at(2,0).a==0);
 pstouch::Image allHidden(1,1,{200,100,50,0});pstouch::auto_contrast(allHidden);assert(allHidden.at(0,0).r==200&&allHidden.at(0,0).g==100&&allHidden.at(0,0).b==50);
 bad=false;try{pstouch::adjust_levels(levels,128,128);}catch(const std::invalid_argument&){bad=true;}assert(bad);


 pstouch::Image tones(2,1,{30,60,90,51});tones.at(1,0)={220,180,140,201};pstouch::adjust_shadows_highlights(tones,0.5f,-0.5f);assert(tones.at(0,0).r>30&&tones.at(1,0).r<220&&tones.at(0,0).a==51&&tones.at(1,0).a==201);
 pstouch::Image balance(1,1,{100,100,100,87});pstouch::color_balance(balance,1.0f,0.0f,-1.0f);assert(balance.at(0,0).r==200&&balance.at(0,0).g==100&&balance.at(0,0).b==50&&balance.at(0,0).a==87);
 bad=false;try{pstouch::adjust_shadows_highlights(tones,2.0f,0.0f);}catch(const std::invalid_argument&){bad=true;}assert(bad);
 bad=false;try{pstouch::color_balance(balance,0.0f,NAN,0.0f);}catch(const std::invalid_argument&){bad=true;}assert(bad);

 pstouch::Image vibrant(2,1,{128,128,128,44});vibrant.at(1,0)={180,120,80,211};auto beforeV=vibrant;pstouch::adjust_vibrance(vibrant,1.0f);assert(vibrant.at(0,0).r==128&&vibrant.at(0,0).g==128&&vibrant.at(0,0).b==128);assert(vibrant.at(1,0).r>beforeV.at(1,0).r&&vibrant.at(1,0).b<beforeV.at(1,0).b);assert(vibrant.at(0,0).a==44&&vibrant.at(1,0).a==211);pstouch::adjust_vibrance(vibrant,-1.0f);assert(std::abs((int)vibrant.at(1,0).r-(int)vibrant.at(1,0).g)<std::abs((int)beforeV.at(1,0).r-(int)beforeV.at(1,0).g));bad=false;try{pstouch::adjust_vibrance(vibrant,1.1f);}catch(const std::invalid_argument&){bad=true;}assert(bad);
 pstouch::Image equalized(3,1,{30,20,10,31});equalized.at(1,0)={60,40,20,127};equalized.at(2,0)={90,60,30,255};auto eqBefore=equalized;pstouch::equalize_luminance(equalized);assert(equalized.at(0,0).r<equalized.at(1,0).r&&equalized.at(1,0).r<equalized.at(2,0).r);assert(equalized.at(0,0).a==31&&equalized.at(1,0).a==127&&equalized.at(2,0).a==255);pstouch::Image constant(2,1,{90,90,90,255});pstouch::equalize_luminance(constant);assert(constant.at(0,0).r==90&&constant.at(1,0).r==90);
 // Fully transparent RGB values must not skew visible-image histogram statistics.
 pstouch::Image visibleOnly(2,1,{30,30,30,255});visibleOnly.at(1,0)={90,90,90,255};
 pstouch::Image withHiddenRgb(3,1,{30,30,30,255});withHiddenRgb.at(1,0)={90,90,90,255};withHiddenRgb.at(2,0)={255,255,255,0};
 pstouch::equalize_luminance(visibleOnly);pstouch::equalize_luminance(withHiddenRgb);
 assert(visibleOnly.at(0,0).r==withHiddenRgb.at(0,0).r&&visibleOnly.at(1,0).r==withHiddenRgb.at(1,0).r);
 assert(withHiddenRgb.at(2,0).r==255&&withHiddenRgb.at(2,0).a==0);
 std::cout<<"PASS: transforms, color filters, 100% core ops, blur, sharpen, edges, gamma, temperature, vignette and pixelation\\n";
}
