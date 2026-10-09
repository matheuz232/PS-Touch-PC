#include "pstouch/document.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <stdexcept>
using namespace pstouch;
int main(){
 Document d(2,2,"Test Document"); Image bottom(2,2,{0,0,255,255}); d.add_layer(Layer("Background",bottom));
 Image top(1,1,{255,0,0,128}); Layer l("Paint",top); l.x=0;l.y=0;d.add_layer(l);
 Image c=d.composite();assert(c.at(0,0).r==128&&c.at(0,0).b==127&&c.at(0,0).a==255);assert(c.at(1,1).b==255);
 d.mutable_layers()[1].visible=false;c=d.composite();assert(c.at(0,0).r==0&&c.at(0,0).b==255);
 d.mutable_layers()[1].visible=true;d.mutable_layers()[1].opacity=128;c=d.composite();assert(c.at(0,0).r==64&&c.at(0,0).b==191);
 d.mutable_layers()[1].opacity=255;d.checkpoint("before rename");d.set_name("Renamed");d.checkpoint("rename");assert(d.undo_label()=="rename");assert(d.undo()&&d.name()=="Test Document");assert(d.redo()&&d.name()=="Renamed");
 d.save("pstouch-test.ptdoc");Document r=Document::load("pstouch-test.ptdoc");assert(r.name()=="Renamed"&&r.width()==2&&r.layers().size()==2);assert(r.layers()[0].name=="Background"&&r.layers()[1].name=="Paint");assert(r.composite().at(0,0).r==128);d.set_name("Saved Again");d.save("pstouch-test.ptdoc");Document overwritten=Document::load("pstouch-test.ptdoc");assert(overwritten.name()=="Saved Again");std::remove("pstouch-test.ptdoc");
 bool threw=false;try{Document::load("not-a-project.ptdoc");}catch(const std::runtime_error&){threw=true;}assert(threw);
 Document stack(1,1);for(int n=0;n<25;++n){stack.set_name(std::to_string(n));stack.checkpoint("step");}int undos=0;while(stack.undo())++undos;assert(undos==20);
 std::cout<<"PASS: layer compositing, visibility, opacity, document round-trip and overwrite, history/redo cap, invalid project rejection\n";
}
