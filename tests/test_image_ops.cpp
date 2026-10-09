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
 std::cout<<"PASS: crop, transforms, color filters, posterize, invert and alpha preservation\\n";
}
