#include "pstouch/psd.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(){pstouch::Image image(2,2,{10,20,30,255});image.at(1,0)={200,100,50,128};pstouch::save_psd_flattened(image,"pstouch-test.psd");std::ifstream f("pstouch-test.psd",std::ios::binary);char sig[4]{};f.read(sig,4);assert(sig[0]=='8'&&sig[1]=='B'&&sig[2]=='P'&&sig[3]=='S');f.seekg(12);unsigned char channels[2]{};f.read(reinterpret_cast<char*>(channels),2);assert(channels[0]==0&&channels[1]==4);f.seekg(22);unsigned char depth[2]{};f.read(reinterpret_cast<char*>(depth),2);assert(depth[0]==0&&depth[1]==8);f.seekg(38);unsigned char compression[2]{};f.read(reinterpret_cast<char*>(compression),2);assert(compression[0]==0&&compression[1]==0);f.seekg(40);unsigned char red[4]{};f.read(reinterpret_cast<char*>(red),4);assert(red[0]==10&&red[1]==200&&red[2]==10&&red[3]==10);f.close();pstouch::Image updated(1,1,{90,80,70,255});pstouch::save_psd_flattened(updated,"pstouch-test.psd");auto reread=pstouch::load_psd_flattened("pstouch-test.psd");assert(reread.width()==1&&reread.height()==1&&reread.at(0,0).r==90);std::remove("pstouch-test.psd");std::cout<<"PASS: PSD v1 header, RGBA channel declaration, raw planar pixel encoding, overwrite\n";}
