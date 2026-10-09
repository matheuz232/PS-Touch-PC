#include "pstouch/image_io.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>
using namespace pstouch;
int main() {
    const char* png="pstouch-io-test.png"; const char* jpg="pstouch-io-test.jpg";
    Image original(3,2,{10,20,30,255}); original.at(1,0)={200,100,50,128}; original.at(2,1)={0,255,100,255};
    save_image(original,png); Image decoded=load_image(png);
    assert(decoded.width()==3 && decoded.height()==2);
    assert(decoded.at(0,0).r==10 && decoded.at(0,0).g==20 && decoded.at(0,0).b==30 && decoded.at(0,0).a==255);
    assert(decoded.at(1,0).r==200 && decoded.at(1,0).a==128);
    save_image(original,jpg,100); Image jpeg=load_image(jpg);
    assert(jpeg.width()==3 && jpeg.height()==2 && jpeg.at(0,0).a==255);
    std::remove(png); std::remove(jpg);
    bool threw=false; try { (void)load_image("bad.bmp"); } catch(const std::invalid_argument&) { threw=true; } assert(threw);
    std::cout<<"PASS: PNG/JPEG encode-decode, alpha preservation for PNG, JPEG opaque decode, unsupported format rejection\n";
}
