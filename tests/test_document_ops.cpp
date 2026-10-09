#include "pstouch/document.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
int main(){pstouch::Document d(3,3);d.add_layer(pstouch::Layer("base",pstouch::Image(1,1,{1,2,3,255})));auto copy=d.duplicate_layer(0);assert(copy==1&&d.layers().size()==2&&d.layers()[1].name=="base copy");d.rename_layer(1,"edited");d.set_layer_visibility(1,false);d.set_layer_opacity(1,128);assert(d.layers()[1].name=="edited"&&!d.layers()[1].visible&&d.layers()[1].opacity==128);d.move_layer(1,0);assert(d.layers()[0].name=="edited");bool bad=false;try{d.rename_layer(8,"x");}catch(const std::out_of_range&){bad=true;}assert(bad);std::cout<<"PASS: layer duplicate/rename/visibility/opacity/reorder and bounds checks\n";}
