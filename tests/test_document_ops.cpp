#include "pstouch/document.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <utility>
int main(){pstouch::Document d(3,3);d.add_layer(pstouch::Layer("base",pstouch::Image(1,1,{1,2,3,255})));auto copy=d.duplicate_layer(0);assert(copy==1&&d.layers().size()==2&&d.layers()[1].name=="base copy");d.rename_layer(1,"edited");d.set_layer_visibility(1,false);d.set_layer_opacity(1,128);assert(d.layers()[1].name=="edited"&&!d.layers()[1].visible&&d.layers()[1].opacity==128);d.move_layer(1,0);assert(d.layers()[0].name=="edited");bool bad=false;try{d.rename_layer(8,"x");}catch(const std::out_of_range&){bad=true;}assert(bad);
 // Composite must respect layer order, offsets and visibility.
 pstouch::Document composite(3,2,"composite");
 composite.add_layer(pstouch::Layer("base",pstouch::Image(3,2,{10,20,30,255})));
 pstouch::Image overlay(1,1,{200,100,50,255});
 pstouch::Layer top("overlay",std::move(overlay)); top.x=1; top.y=0;
 composite.add_layer(std::move(top));
 auto merged=composite.composite();
 assert(merged.at(0,0).r==10 && merged.at(0,0).g==20);
 assert(merged.at(1,0).r==200 && merged.at(1,0).g==100 && merged.at(1,0).b==50);
 composite.set_layer_visibility(1,false);
 merged=composite.composite();
 assert(merged.at(1,0).r==10 && merged.at(1,0).g==20 && merged.at(1,0).b==30);
 // Checkpoints restore document state and support redo.
 composite.checkpoint("before layer edit");
 composite.set_layer_visibility(1,true);
 composite.checkpoint("show overlay");
 assert(composite.can_undo() && composite.undo());
 assert(!composite.layers()[1].visible);
 assert(composite.can_redo() && composite.redo());
 assert(composite.layers()[1].visible);

 std::cout<<"PASS: layer operations, composite ordering/offset/visibility, history undo/redo and bounds checks\n";}
