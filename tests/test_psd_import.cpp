#include "pstouch/psd.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
static void be16(std::ofstream& o,unsigned v){o.put(static_cast<char>(v>>8));o.put(static_cast<char>(v));}
static void be32(std::ofstream& o,unsigned v){o.put(static_cast<char>(v>>24));o.put(static_cast<char>(v>>16));o.put(static_cast<char>(v>>8));o.put(static_cast<char>(v));}
static void header(std::ofstream& o,unsigned ch,unsigned w,unsigned h,unsigned compression){o.write("8BPS",4);be16(o,1);char r[6]={};o.write(r,6);be16(o,ch);be32(o,h);be32(o,w);be16(o,8);be16(o,3);be32(o,0);be32(o,0);be32(o,0);be16(o,compression);}
int main(){
 {pstouch::Image src(2,2,{1,2,3,255});src.at(1,0)={20,30,40,80};pstouch::save_psd_flattened(src,"roundtrip.psd");auto got=pstouch::load_psd_flattened("roundtrip.psd");assert(got.width()==2&&got.height()==2);assert(got.at(1,0).r==20&&got.at(1,0).g==30&&got.at(1,0).b==40&&got.at(1,0).a==80);std::remove("roundtrip.psd");}
 {std::ofstream o("rle.psd",std::ios::binary);header(o,3,2,1,1);be16(o,3);be16(o,3);be16(o,3);unsigned char row[]={1,10,20,1,30,40,1,50,60};o.write(reinterpret_cast<char*>(row),sizeof(row));o.close();auto got=pstouch::load_psd_flattened("rle.psd");assert(got.at(0,0).r==10&&got.at(1,0).r==20&&got.at(0,0).g==30&&got.at(1,0).b==60);std::remove("rle.psd");}
 {std::ofstream o("bad.psd",std::ios::binary);o.write("nope",4);o.close();bool rejected=false;try{(void)pstouch::load_psd_flattened("bad.psd");}catch(const std::exception&){rejected=true;}assert(rejected);std::remove("bad.psd");}
 {std::ofstream o("oversize-rle.psd",std::ios::binary);header(o,3,2,1,1);be16(o,65535);be16(o,3);be16(o,3);o.close();bool rejected=false;try{(void)pstouch::load_psd_flattened("oversize-rle.psd");}catch(const std::exception&){rejected=true;}assert(rejected);std::remove("oversize-rle.psd");}
 {const std::string path=u8"pstouch-psd-a\u00e7\u00e3o-\u65e5\u672c.psd";pstouch::Image src(1,1,{7,8,9,10});pstouch::save_psd_flattened(src,path);auto first=pstouch::load_psd_flattened(path);assert(first.at(0,0).r==7&&first.at(0,0).a==10);pstouch::Image replacement(1,1,{90,80,70,60});pstouch::save_psd_flattened(replacement,path);auto second=pstouch::load_psd_flattened(path);assert(second.at(0,0).r==90&&second.at(0,0).a==60);std::remove(path.c_str());}\n std::cout<<"PASS: PSD flattened raw/RLE import, RGBA round-trip, Unicode paths, overwrite, malformed and oversized RLE rejection\n";
}
